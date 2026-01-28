#include "wave/TilemapLoader.hpp"

#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace wave {
namespace {

constexpr const char* kAtlasImageName = "16k-waves-trans-atlas.png";

std::optional<std::string> ReadTextFile(const std::filesystem::path& path) {
    FILE* file = std::fopen(path.string().c_str(), "rb");
    if (!file) {
        return std::nullopt;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    std::string contents;
    if (size > 0) {
        contents.resize(static_cast<size_t>(size));
        const size_t readSize = std::fread(contents.data(), 1, contents.size(), file);
        if (readSize != contents.size()) {
            std::fclose(file);
            return std::nullopt;
        }
    }
    std::fclose(file);
    return contents;
}

int Base64Value(unsigned char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return -1;
}

std::vector<uint8_t> DecodeBase64(const std::string& input) {
    std::vector<uint8_t> output;
    int value = 0;
    int bits = -8;
    for (unsigned char c : input) {
        if (std::isspace(c) != 0) {
            continue;
        }
        if (c == '=') {
            break;
        }
        const int decoded = Base64Value(c);
        if (decoded < 0) {
            continue;
        }
        value = (value << 6) + decoded;
        bits += 6;
        if (bits >= 0) {
            output.push_back(static_cast<uint8_t>((value >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return output;
}

bool DecompressZlib(const std::vector<uint8_t>& input, size_t expectedSize,
    std::vector<uint8_t>* output) {
    output->assign(expectedSize, 0);
    uLongf destLen = static_cast<uLongf>(expectedSize);
    const int result = uncompress(output->data(), &destLen, input.data(),
        static_cast<uLongf>(input.size()));
    if (result != Z_OK) {
        return false;
    }
    output->resize(static_cast<size_t>(destLen));
    return true;
}

bool ExtractLayers(const nlohmann::json& doc, int width, int height,
    std::vector<ChunkLayer>* layers, std::string* error) {
    const size_t expectedTiles = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t expectedBytes = expectedTiles * 4;
    if (!doc.contains("layers") || !doc["layers"].is_array()) {
        if (error) {
            *error = "Tilemap JSON missing layers array.";
        }
        return false;
    }

    size_t layerIndex = 0;
    for (const auto& layer : doc["layers"]) {
        if (!layer.contains("type") || !layer["type"].is_string()) {
            ++layerIndex;
            continue;
        }
        if (layer["type"].get<std::string>() != "tilelayer") {
            ++layerIndex;
            continue;
        }
        if (!layer.contains("data")) {
            ++layerIndex;
            continue;
        }

        const std::string name = layer.contains("name") && layer["name"].is_string()
            ? layer["name"].get<std::string>()
            : "layer";

        std::vector<uint32_t> gids;
        gids.reserve(expectedTiles);

        if (layer["data"].is_array()) {
            const auto& dataArray = layer["data"];
            if (dataArray.size() != expectedTiles) {
                if (error) {
                    *error = "Layer '" + name + "' has unexpected tile count.";
                }
                return false;
            }
            for (const auto& entry : dataArray) {
                if (!entry.is_number()) {
                    if (error) {
                        *error = "Layer '" + name + "' has non-numeric data.";
                    }
                    return false;
                }
                gids.push_back(entry.get<uint32_t>());
            }
        } else if (layer["data"].is_string()) {
            const std::string encoding =
                layer.contains("encoding") && layer["encoding"].is_string()
                ? layer["encoding"].get<std::string>()
                : "";
            const std::optional<std::string> compression =
                layer.contains("compression") && layer["compression"].is_string()
                ? std::optional<std::string>(layer["compression"].get<std::string>())
                : std::nullopt;

            if (encoding != "base64") {
                if (error) {
                    *error = "Layer '" + name + "' unsupported encoding: " + encoding;
                }
                return false;
            }

            const std::string dataStr = layer["data"].get<std::string>();
            const std::vector<uint8_t> decoded = DecodeBase64(dataStr);
            std::vector<uint8_t> raw;
            if (compression && *compression == "zlib") {
                if (!DecompressZlib(decoded, expectedBytes, &raw)) {
                    if (error) {
                        *error = "Layer '" + name + "' zlib decompress failed.";
                    }
                    return false;
                }
            } else if (!compression || compression->empty()) {
                raw = decoded;
            } else {
                if (error) {
                    *error = "Layer '" + name + "' unsupported compression: " + *compression;
                }
                return false;
            }

            if (raw.size() != expectedBytes) {
                if (error) {
                    *error = "Layer '" + name + "' size mismatch.";
                }
                return false;
            }

            for (size_t i = 0; i + 3 < raw.size(); i += 4) {
                const uint32_t gid = static_cast<uint32_t>(raw[i]) |
                    (static_cast<uint32_t>(raw[i + 1]) << 8) |
                    (static_cast<uint32_t>(raw[i + 2]) << 16) |
                    (static_cast<uint32_t>(raw[i + 3]) << 24);
                gids.push_back(gid);
            }
        } else {
            if (error) {
                *error = "Layer '" + name + "' data is neither array nor string.";
            }
            return false;
        }

        layers->push_back({name, std::move(gids)});
        ++layerIndex;
    }

    return !layers->empty();
}

}  // namespace

std::optional<ChunkMap> LoadChunkMapFromJson(const std::filesystem::path& path,
    const ChunkId& id, std::string* error) {
    auto jsonText = ReadTextFile(path);
    if (!jsonText) {
        if (error) {
            *error = "Failed to read chunk JSON: " + path.string();
        }
        return std::nullopt;
    }

    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(*jsonText);
    } catch (const std::exception& ex) {
        if (error) {
            *error = std::string("Failed to parse chunk JSON: ") + ex.what();
        }
        return std::nullopt;
    }

    auto getInt = [&](const nlohmann::json& object, const char* key) -> std::optional<int> {
        if (!object.contains(key) || !object[key].is_number()) {
            return std::nullopt;
        }
        return object[key].get<int>();
    };

    auto mapWidth = getInt(doc, "width");
    auto mapHeight = getInt(doc, "height");
    auto tileWidth = getInt(doc, "tilewidth");
    auto tileHeight = getInt(doc, "tileheight");
    if (!mapWidth || !mapHeight || !tileWidth || !tileHeight) {
        if (error) {
            *error = "Chunk JSON missing width/height/tile size.";
        }
        return std::nullopt;
    }

    if (!doc.contains("tilesets") || !doc["tilesets"].is_array()) {
        if (error) {
            *error = "Chunk JSON missing tilesets array.";
        }
        return std::nullopt;
    }

    AtlasInfo atlas;
    bool foundAtlas = false;
    for (const auto& tileset : doc["tilesets"]) {
        if (!tileset.is_object()) {
            continue;
        }
        const auto firstGid = getInt(tileset, "firstgid");
        const auto columns = getInt(tileset, "columns");
        const auto tsTileWidth = getInt(tileset, "tilewidth");
        const auto tsTileHeight = getInt(tileset, "tileheight");
        const auto tileCount = getInt(tileset, "tilecount");
        const std::string image = tileset.contains("image") && tileset["image"].is_string()
            ? tileset["image"].get<std::string>()
            : "";
        if (image.empty() || !firstGid || !columns || !tsTileWidth || !tsTileHeight
            || !tileCount) {
            continue;
        }
        const std::filesystem::path imagePath(image);
        if (imagePath.filename() != kAtlasImageName) {
            continue;
        }
        atlas.firstGid = *firstGid;
        atlas.columns = *columns;
        atlas.tileWidth = *tsTileWidth;
        atlas.tileHeight = *tsTileHeight;
        atlas.tileCount = *tileCount;
        foundAtlas = true;
        break;
    }
    if (!foundAtlas) {
        if (error) {
            *error = "Chunk JSON missing atlas tileset: " + std::string(kAtlasImageName);
        }
        return std::nullopt;
    }

    std::vector<ChunkLayer> layers;
    if (!ExtractLayers(doc, *mapWidth, *mapHeight, &layers, error)) {
        return std::nullopt;
    }

    ChunkMap map;
    map.id = id;
    map.width = *mapWidth;
    map.height = *mapHeight;
    map.tileWidth = *tileWidth;
    map.tileHeight = *tileHeight;
    map.atlas = atlas;
    map.layers = std::move(layers);
    return map;
}

}  // namespace wave
