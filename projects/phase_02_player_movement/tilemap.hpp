#pragma once

#include <SDL.h>
#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
struct TileLayer {
    std::string name;
    std::vector<uint32_t> gids;
    bool hasBounds = false;
    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    bool hasTileset = false;
    int tilesetFirstGid = 0;
};

struct TilesetInfo {
    int firstGid = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    int columns = 0;
    int tileCount = 0;
    std::string image;
};

struct AtlasInfo {
    int firstGid = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    int columns = 0;
    int tileCount = 0;
};

struct Tilemap {
    int width = 0;
    int height = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    AtlasInfo atlas;
    std::vector<TilesetInfo> tilesets;
    std::vector<TileLayer> layers;
};

namespace {

constexpr const char* kAtlasImageName = "16k-waves-trans-atlas.png";
constexpr bool kDebugOverlay = false;

inline SDL_Rect SrcRectForLocalIndex(const AtlasInfo& atlas, uint32_t localIndex) {
    const int col = static_cast<int>(localIndex % static_cast<uint32_t>(atlas.columns));
    const int row = static_cast<int>(localIndex / static_cast<uint32_t>(atlas.columns));
    return SDL_Rect{col * atlas.tileWidth, row * atlas.tileHeight, atlas.tileWidth,
        atlas.tileHeight};
}

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

const TilesetInfo* SelectTilesetByFirstGid(uint32_t gid,
    const std::vector<TilesetInfo>& tilesets) {
    const TilesetInfo* selected = nullptr;
    for (const auto& tileset : tilesets) {
        if (gid < static_cast<uint32_t>(tileset.firstGid)) {
            continue;
        }
        if (!selected || tileset.firstGid > selected->firstGid) {
            selected = &tileset;
        }
    }
    return selected;
}

bool ExtractLayers(const nlohmann::json& doc, int width, int height,
    const std::vector<TilesetInfo>& tilesets, std::vector<TileLayer>* layers) {
    const size_t expectedSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    if (!doc.contains("layers") || !doc["layers"].is_array()) {
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
        if (!layer.contains("data") || !layer["data"].is_string()) {
            ++layerIndex;
            continue;
        }

        const std::string name = layer.contains("name") && layer["name"].is_string()
            ? layer["name"].get<std::string>()
            : "layer";
        const std::string encoding =
            layer.contains("encoding") && layer["encoding"].is_string()
            ? layer["encoding"].get<std::string>()
            : "";
        const std::optional<std::string> compression =
            layer.contains("compression") && layer["compression"].is_string()
            ? std::optional<std::string>(layer["compression"].get<std::string>())
            : std::nullopt;

        const std::string dataStr = layer["data"].get<std::string>();
        if (kDebugOverlay) {
            const std::string prefix = dataStr.substr(0, std::min<size_t>(8, dataStr.size()));
            std::cout << "Layer " << layerIndex << " '" << name << "' encoding=" << encoding
                      << " compression=" << (compression ? *compression : "none")
                      << " base64_len=" << dataStr.size() << " prefix=" << prefix << "\n";
        }

        if (encoding != "base64") {
            std::cerr << "Tile layer '" << name << "' unsupported encoding: " << encoding << "\n";
            return false;
        }

        const std::vector<uint8_t> decoded = DecodeBase64(dataStr);
        std::vector<uint8_t> raw;
        if (compression && *compression == "zlib") {
            if (!DecompressZlib(decoded, expectedSize, &raw)) {
                std::cerr << "Tile layer '" << name << "' decompress failed at index "
                          << layerIndex << "\n";
                return false;
            }
        } else if (!compression || compression->empty()) {
            raw = decoded;
        } else {
            std::cerr << "Tile layer '" << name
                      << "' unsupported compression: " << *compression << "\n";
            return false;
        }

        if (raw.size() != expectedSize) {
            std::cerr << "Tile layer '" << name << "' size mismatch at index " << layerIndex
                      << " expected " << expectedSize << " got " << raw.size() << "\n";
            return false;
        }

        std::vector<uint32_t> gids;
        gids.reserve(static_cast<size_t>(width) * static_cast<size_t>(height));
        uint32_t minGid = std::numeric_limits<uint32_t>::max();
        uint32_t maxGid = 0;
        size_t nonZeroCount = 0;
        int localMin = std::numeric_limits<int>::max();
        int localMax = std::numeric_limits<int>::min();
        size_t outOfRangeCount = 0;
        const TilesetInfo* selectedTileset = nullptr;
        bool hasBounds = false;
        int minX = 0;
        int minY = 0;
        int maxX = 0;
        int maxY = 0;
        for (size_t i = 0; i + 3 < expectedSize; i += 4) {
            const uint32_t gid = static_cast<uint32_t>(raw[i]) |
                (static_cast<uint32_t>(raw[i + 1]) << 8) |
                (static_cast<uint32_t>(raw[i + 2]) << 16) |
                (static_cast<uint32_t>(raw[i + 3]) << 24);
            gids.push_back(gid);
            if (gid != 0) {
                minGid = std::min(minGid, gid);
                maxGid = std::max(maxGid, gid);
                ++nonZeroCount;
                const size_t tileIndex = gids.size() - 1;
                const int x = static_cast<int>(tileIndex % static_cast<size_t>(width));
                const int y = static_cast<int>(tileIndex / static_cast<size_t>(width));
                if (!hasBounds) {
                    minX = maxX = x;
                    minY = maxY = y;
                    hasBounds = true;
                } else {
                    minX = std::min(minX, x);
                    minY = std::min(minY, y);
                    maxX = std::max(maxX, x);
                    maxY = std::max(maxY, y);
                }
            }
        }

        if (maxGid == 0) {
            minGid = 0;
        }
        if (maxGid > 0) {
            selectedTileset = SelectTilesetByFirstGid(minGid, tilesets);
        }

        if (selectedTileset && nonZeroCount > 0) {
            for (const uint32_t gid : gids) {
                if (gid == 0) {
                    continue;
                }
                const int local = static_cast<int>(gid) - selectedTileset->firstGid;
                localMin = std::min(localMin, local);
                localMax = std::max(localMax, local);
                if (local < 0 || local >= selectedTileset->tileCount) {
                    ++outOfRangeCount;
                }
            }
        }

        if (kDebugOverlay) {
            std::cout << "Layer '" << name << "' minGid=" << minGid << " maxGid=" << maxGid
                      << " nonzero=" << nonZeroCount;
            if (selectedTileset) {
                std::cout << " tileset_firstgid=" << selectedTileset->firstGid
                          << " local_range=["
                          << (localMin == std::numeric_limits<int>::max() ? 0 : localMin)
                          << ","
                          << (localMax == std::numeric_limits<int>::min() ? 0 : localMax)
                          << "]"
                          << " tilecount=" << selectedTileset->tileCount
                          << " out_of_range=" << outOfRangeCount;
            } else {
                std::cout << " tileset_firstgid=none";
            }
            if (hasBounds) {
                std::cout << " bounds=(" << minX << "," << minY << ")-(" << maxX << "," << maxY
                          << ")";
            }
            std::cout << "\n";
        }
        TileLayer layerInfo;
        layerInfo.name = name;
        layerInfo.gids = std::move(gids);
        layerInfo.hasBounds = hasBounds;
        layerInfo.minX = minX;
        layerInfo.minY = minY;
        layerInfo.maxX = maxX;
        layerInfo.maxY = maxY;
        layerInfo.hasTileset = (selectedTileset != nullptr);
        layerInfo.tilesetFirstGid = selectedTileset ? selectedTileset->firstGid : 0;
        layers->push_back(std::move(layerInfo));
        ++layerIndex;
    }

    return !layers->empty();
}

}  // namespace

std::optional<Tilemap> LoadTilemapFromJson(const std::filesystem::path& path,
    std::string* error) {
    auto jsonText = ReadTextFile(path);
    if (!jsonText) {
        if (error) {
            *error = "Failed to read tilemap JSON: " + path.string();
        }
        return std::nullopt;
    }

    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(*jsonText);
    } catch (const std::exception& ex) {
        if (error) {
            *error = std::string("Failed to parse tilemap JSON: ") + ex.what();
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
            *error = "Tilemap JSON missing width/height/tile size.";
        }
        return std::nullopt;
    }

    if (!doc.contains("tilesets") || !doc["tilesets"].is_array()) {
        if (error) {
            *error = "Tilemap JSON missing tilesets array.";
        }
        return std::nullopt;
    }
    if (kDebugOverlay) {
        std::cout << "Tilemap: size=" << *mapWidth << "x" << *mapHeight
                  << " tile=" << *tileWidth << "x" << *tileHeight
                  << " layers=" << doc["layers"].size()
                  << " tilesets=" << doc["tilesets"].size() << "\n";
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
            const std::string source = tileset.contains("source") && tileset["source"].is_string()
                ? tileset["source"].get<std::string>()
                : "";
            std::cout << "Tileset firstgid=" << (firstGid ? *firstGid : 0)
                      << " image=" << (image.empty() ? "none" : image)
                      << " source=" << (source.empty() ? "none" : source)
                      << " columns=" << (columns ? *columns : 0)
                      << " tilecount=" << (tileCount ? *tileCount : 0)
                      << " tile=" << (tsTileWidth ? *tsTileWidth : 0) << "x"
                      << (tsTileHeight ? *tsTileHeight : 0) << "\n";
        }
    }

    std::vector<TilesetInfo> tilesets;
    for (const auto& tileset : doc["tilesets"]) {
        if (!tileset.is_object()) {
            continue;
        }
        const auto firstGid = getInt(tileset, "firstgid");
        const auto columns = getInt(tileset, "columns");
        const auto tileWidth = getInt(tileset, "tilewidth");
        const auto tileHeight = getInt(tileset, "tileheight");
        const auto tileCount = getInt(tileset, "tilecount");
        const std::string image = tileset.contains("image") && tileset["image"].is_string()
            ? tileset["image"].get<std::string>()
            : "";

        if (!firstGid || !columns || !tileWidth || !tileHeight || !tileCount || image.empty()) {
            continue;
        }

        TilesetInfo info;
        info.firstGid = *firstGid;
        info.columns = *columns;
        info.tileWidth = *tileWidth;
        info.tileHeight = *tileHeight;
        info.tileCount = *tileCount;
        info.image = image;
        tilesets.push_back(std::move(info));
    }

    if (tilesets.empty()) {
        if (error) {
            *error = "Tilemap JSON missing usable tilesets.";
        }
        return std::nullopt;
    }

    std::sort(tilesets.begin(), tilesets.end(),
        [](const TilesetInfo& a, const TilesetInfo& b) { return a.firstGid < b.firstGid; });

    const TilesetInfo* atlas = nullptr;
    for (const auto& tileset : tilesets) {
        const std::filesystem::path imagePath(tileset.image);
        if (imagePath.filename() == kAtlasImageName) {
            atlas = &tileset;
            break;
        }
    }
    if (!atlas) {
        if (error) {
            *error = std::string("Tilemap JSON missing tileset image: ") + kAtlasImageName;
        }
        return std::nullopt;
    }
    if (kDebugOverlay && atlas->firstGid != 250001) {
        std::cout << "Atlas tileset firstgid=" << atlas->firstGid
                  << " (expected 250001)\n";
    }

    Tilemap map;
    map.width = *mapWidth;
    map.height = *mapHeight;
    map.tileWidth = *tileWidth;
    map.tileHeight = *tileHeight;
    map.atlas.firstGid = atlas->firstGid;
    map.atlas.columns = atlas->columns;
    map.atlas.tileWidth = atlas->tileWidth;
    map.atlas.tileHeight = atlas->tileHeight;
    map.atlas.tileCount = atlas->tileCount;
    map.tilesets = tilesets;

    if (!ExtractLayers(doc, map.width, map.height, map.tilesets, &map.layers)) {
        if (error) {
            *error = "Failed to extract tile layers from JSON.";
        }
        return std::nullopt;
    }

    if (!kDebugOverlay) {
        std::cout << "Tilemap: size=" << map.width << "x" << map.height
                  << " atlas_firstgid=" << map.atlas.firstGid
                  << " layers=" << map.layers.size() << "\n";
    }

    return map;
}

int RenderTilemap(SDL_Renderer* renderer, SDL_Texture* atlas, const Tilemap& map,
    float viewOffsetX, float viewOffsetY, float viewScale) {
    if (map.atlas.tileCount <= 0) {
        return 0;
    }

    const uint32_t atlasFirst = static_cast<uint32_t>(map.atlas.firstGid);
    const uint32_t atlasLast =
        atlasFirst + static_cast<uint32_t>(map.atlas.tileCount - 1);
    const float scale = std::max(0.01f, viewScale);
    SDL_Rect src{0, 0, map.atlas.tileWidth, map.atlas.tileHeight};
    SDL_Rect dst{0, 0,
        static_cast<int>(static_cast<float>(map.tileWidth) * scale),
        static_cast<int>(static_cast<float>(map.tileHeight) * scale)};
    int outputW = 0;
    int outputH = 0;
    if (SDL_GetRendererOutputSize(renderer, &outputW, &outputH) != 0) {
        if (kDebugOverlay) {
            std::cerr << "SDL_GetRendererOutputSize failed: " << SDL_GetError() << "\n";
        }
        outputW = map.width * map.tileWidth;
        outputH = map.height * map.tileHeight;
    }
    const int originWorldX = (map.width / 2) * map.tileWidth;
    const int originWorldY = (map.height / 2) * map.tileHeight;
    const float screenCenterX = static_cast<float>(outputW) * 0.5f;
    const float screenCenterY = static_cast<float>(outputH) * 0.5f;
    int tilesDrawn = 0;
    static bool loggedSamples = false;
    int samplesLogged = 0;
    int focusWorldX = originWorldX;
    int focusWorldY = originWorldY;
    for (const auto& layer : map.layers) {
        if (!layer.hasBounds) {
            continue;
        }
        if (!layer.hasTileset || layer.tilesetFirstGid != map.atlas.firstGid) {
            continue;
        }
        const int centerX = (layer.minX + layer.maxX) / 2;
        const int centerY = (layer.minY + layer.maxY) / 2;
        focusWorldX = centerX * map.tileWidth;
        focusWorldY = centerY * map.tileHeight;
        break;
    }

    for (const TileLayer& layer : map.layers) {
        const size_t tileCount = static_cast<size_t>(map.width) * static_cast<size_t>(map.height);
        if (layer.gids.size() < tileCount) {
            continue;
        }
        for (size_t index = 0; index < tileCount; ++index) {
            const uint32_t gid = layer.gids[index];
            if (gid == 0) {
                continue;
            }
            if (gid < atlasFirst || gid > atlasLast) {
                continue;
            }
            const uint32_t tileIndex = gid - atlasFirst;
            src = SrcRectForLocalIndex(map.atlas, tileIndex);

            const int tileX = static_cast<int>(index % static_cast<size_t>(map.width));
            const int tileY = static_cast<int>(index / static_cast<size_t>(map.width));
            const float worldX = static_cast<float>(tileX * map.tileWidth - focusWorldX);
            const float worldY = static_cast<float>(tileY * map.tileHeight - focusWorldY);
            dst.x = static_cast<int>(worldX * scale + screenCenterX + viewOffsetX);
            dst.y = static_cast<int>(worldY * scale + screenCenterY + viewOffsetY);

            SDL_RenderCopy(renderer, atlas, &src, &dst);
            ++tilesDrawn;
            if (kDebugOverlay && !loggedSamples && samplesLogged < 10) {
                std::cout << "SampleTile layer=" << layer.name << " gid=" << gid
                          << " local=" << tileIndex << " src={" << src.x << "," << src.y << ","
                          << src.w << "," << src.h << "} dst={" << dst.x << "," << dst.y << ","
                          << dst.w << "," << dst.h << "}\n";
                ++samplesLogged;
                if (samplesLogged == 10) {
                    loggedSamples = true;
                }
            }
        }
    }
    if (kDebugOverlay && !loggedSamples && samplesLogged > 0) {
        loggedSamples = true;
    }
    return tilesDrawn;
}
