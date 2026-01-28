#include "wave/CollisionMap.hpp"

#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <limits>

#include <nlohmann/json.hpp>

namespace wave {
namespace {

constexpr const char* kAtlasImageName = "16k-waves-trans-atlas.png";
constexpr uint32_t kGidFlipMask = 0xE0000000;
constexpr uint32_t kGidIdMask = 0x1FFFFFFF;

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

uint32_t MaskGid(uint32_t gid, bool* hadFlip) {
    if ((gid & kGidFlipMask) != 0u) {
        if (hadFlip) {
            *hadFlip = true;
        }
        gid &= kGidIdMask;
    }
    return gid;
}

void UpdateBounds(Polygon* poly) {
    if (!poly || poly->points.empty()) {
        return;
    }
    float minX = poly->points[0].x;
    float maxX = poly->points[0].x;
    float minY = poly->points[0].y;
    float maxY = poly->points[0].y;
    for (const auto& pt : poly->points) {
        minX = std::min(minX, pt.x);
        minY = std::min(minY, pt.y);
        maxX = std::max(maxX, pt.x);
        maxY = std::max(maxY, pt.y);
    }
    poly->bounds = {minX, minY, maxX, maxY};
}

AABB OffsetBounds(const AABB& bounds, const Vec2& offset, float expand) {
    return {bounds.minX + offset.x - expand, bounds.minY + offset.y - expand,
        bounds.maxX + offset.x + expand, bounds.maxY + offset.y + expand};
}

}  // namespace

std::optional<CollisionMap> LoadCollisionMapFromJson(const std::filesystem::path& path,
    std::string* error) {
    auto jsonText = ReadTextFile(path);
    if (!jsonText) {
        if (error) {
            *error = "Failed to read collision JSON: " + path.string();
        }
        return std::nullopt;
    }

    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(*jsonText);
    } catch (const std::exception& ex) {
        if (error) {
            *error = std::string("Failed to parse collision JSON: ") + ex.what();
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
            *error = "Collision JSON missing width/height/tile size.";
        }
        return std::nullopt;
    }

    if (!doc.contains("tilesets") || !doc["tilesets"].is_array()) {
        if (error) {
            *error = "Collision JSON missing tilesets array.";
        }
        return std::nullopt;
    }

    int tilesetFirstGid = 0;
    std::unordered_map<int, std::vector<Polygon>> tilePolygons;
    bool foundTileset = false;
    for (const auto& tileset : doc["tilesets"]) {
        if (!tileset.is_object()) {
            continue;
        }
        const auto firstGid = getInt(tileset, "firstgid");
        const std::string image = tileset.contains("image") && tileset["image"].is_string()
            ? tileset["image"].get<std::string>()
            : "";
        if (!firstGid || image.empty()) {
            continue;
        }
        const std::filesystem::path imagePath(image);
        if (imagePath.filename() != kAtlasImageName) {
            continue;
        }
        tilesetFirstGid = *firstGid;
        if (tileset.contains("tiles") && tileset["tiles"].is_array()) {
            for (const auto& tile : tileset["tiles"]) {
                if (!tile.is_object()) {
                    continue;
                }
                if (!tile.contains("id") || !tile["id"].is_number()) {
                    continue;
                }
                const int tileId = tile["id"].get<int>();
                if (!tile.contains("objectgroup") || !tile["objectgroup"].is_object()) {
                    continue;
                }
                const auto& objGroup = tile["objectgroup"];
                if (!objGroup.contains("objects") || !objGroup["objects"].is_array()) {
                    continue;
                }
                for (const auto& obj : objGroup["objects"]) {
                    if (!obj.is_object()) {
                        continue;
                    }
                    if (!obj.contains("polygon") || !obj["polygon"].is_array()) {
                        continue;
                    }
                    const float objX = obj.contains("x") && obj["x"].is_number()
                        ? obj["x"].get<float>()
                        : 0.0f;
                    const float objY = obj.contains("y") && obj["y"].is_number()
                        ? obj["y"].get<float>()
                        : 0.0f;
                    Polygon poly;
                    for (const auto& point : obj["polygon"]) {
                        if (!point.is_object()) {
                            continue;
                        }
                        const float px = point.contains("x") && point["x"].is_number()
                            ? point["x"].get<float>()
                            : 0.0f;
                        const float py = point.contains("y") && point["y"].is_number()
                            ? point["y"].get<float>()
                            : 0.0f;
                        poly.points.push_back({objX + px, objY + py});
                    }
                    if (!poly.points.empty()) {
                        UpdateBounds(&poly);
                        tilePolygons[tileId].push_back(std::move(poly));
                    }
                }
            }
        }
        foundTileset = true;
        break;
    }

    if (!foundTileset) {
        if (error) {
            *error = "Collision JSON missing atlas tileset: " + std::string(kAtlasImageName);
        }
        return std::nullopt;
    }

    std::vector<std::vector<int>> cellTiles(
        static_cast<size_t>(*mapWidth) * static_cast<size_t>(*mapHeight));
    bool hasBounds = false;
    int minTileX = 0;
    int maxTileX = 0;
    int minTileY = 0;
    int maxTileY = 0;

    if (doc.contains("layers") && doc["layers"].is_array()) {
        bool loggedFlip = false;
        for (const auto& layer : doc["layers"]) {
            if (!layer.contains("type") || !layer["type"].is_string()) {
                continue;
            }
            if (layer["type"].get<std::string>() != "tilelayer") {
                continue;
            }
            if (!layer.contains("data") || !layer["data"].is_string()) {
                continue;
            }
            const std::string encoding =
                layer.contains("encoding") && layer["encoding"].is_string()
                ? layer["encoding"].get<std::string>()
                : "";
            const std::optional<std::string> compression =
                layer.contains("compression") && layer["compression"].is_string()
                ? std::optional<std::string>(layer["compression"].get<std::string>())
                : std::nullopt;
            if (encoding != "base64") {
                continue;
            }
            const std::string dataStr = layer["data"].get<std::string>();
            const std::vector<uint8_t> decoded = DecodeBase64(dataStr);
            std::vector<uint8_t> raw;
            const size_t expectedBytes =
                static_cast<size_t>(*mapWidth) * static_cast<size_t>(*mapHeight) * 4;
            if (compression && *compression == "zlib") {
                if (!DecompressZlib(decoded, expectedBytes, &raw)) {
                    if (error) {
                        *error = "Collision layer decompress failed.";
                    }
                    return std::nullopt;
                }
            } else if (!compression || compression->empty()) {
                raw = decoded;
            } else {
                if (error) {
                    *error = "Collision layer unsupported compression.";
                }
                return std::nullopt;
            }
            if (raw.size() != expectedBytes) {
                if (error) {
                    *error = "Collision layer size mismatch.";
                }
                return std::nullopt;
            }

            size_t index = 0;
            for (size_t i = 0; i + 3 < raw.size(); i += 4, ++index) {
                uint32_t gid = static_cast<uint32_t>(raw[i]) |
                    (static_cast<uint32_t>(raw[i + 1]) << 8) |
                    (static_cast<uint32_t>(raw[i + 2]) << 16) |
                    (static_cast<uint32_t>(raw[i + 3]) << 24);
                bool hadFlip = false;
                gid = MaskGid(gid, &hadFlip);
                if (hadFlip && !loggedFlip) {
                    loggedFlip = true;
                    std::cerr << "Collision map contains flipped tiles; flips ignored."
                              << "\n";
                }
                if (gid == 0 || gid < static_cast<uint32_t>(tilesetFirstGid)) {
                    continue;
                }
                const int tileId = static_cast<int>(gid) - tilesetFirstGid;
                if (tilePolygons.find(tileId) == tilePolygons.end()) {
                    continue;
                }
                cellTiles[index].push_back(tileId);
                const int tx = static_cast<int>(index % static_cast<size_t>(*mapWidth));
                const int ty = static_cast<int>(index / static_cast<size_t>(*mapWidth));
                if (!hasBounds) {
                    minTileX = maxTileX = tx;
                    minTileY = maxTileY = ty;
                    hasBounds = true;
                } else {
                    minTileX = std::min(minTileX, tx);
                    maxTileX = std::max(maxTileX, tx);
                    minTileY = std::min(minTileY, ty);
                    maxTileY = std::max(maxTileY, ty);
                }
            }
        }
    }

    CollisionMap map;
    map.width = *mapWidth;
    map.height = *mapHeight;
    map.tileWidth = *tileWidth;
    map.tileHeight = *tileHeight;
    if (hasBounds) {
        const float centerTileX = (static_cast<float>(minTileX + maxTileX)) * 0.5f;
        const float centerTileY = (static_cast<float>(minTileY + maxTileY)) * 0.5f;
        map.originWorldX = static_cast<int>(-centerTileX * *tileWidth);
        map.originWorldY = static_cast<int>(-centerTileY * *tileHeight);
    } else {
        map.originWorldX = -(*mapWidth * *tileWidth) / 2;
        map.originWorldY = -(*mapHeight * *tileHeight) / 2;
    }
    map.tilesetFirstGid = tilesetFirstGid;
    map.tilePolygons = std::move(tilePolygons);
    map.cellTiles = std::move(cellTiles);
    return map;
}

float PointSegmentDistanceSquared(const Vec2& p, const Vec2& a, const Vec2& b) {
    const float vx = b.x - a.x;
    const float vy = b.y - a.y;
    const float wx = p.x - a.x;
    const float wy = p.y - a.y;
    const float lenSq = vx * vx + vy * vy;
    if (lenSq <= 0.0001f) {
        const float dx = p.x - a.x;
        const float dy = p.y - a.y;
        return dx * dx + dy * dy;
    }
    float t = (wx * vx + wy * vy) / lenSq;
    t = std::max(0.0f, std::min(1.0f, t));
    const float closestX = a.x + t * vx;
    const float closestY = a.y + t * vy;
    const float dx = p.x - closestX;
    const float dy = p.y - closestY;
    return dx * dx + dy * dy;
}

bool CirclePolygonIntersect(const Vec2& center, float radius, const Polygon& poly,
    const Vec2& offset) {
    const float radiusSq = radius * radius;
    const AABB circleBounds{center.x - radius, center.y - radius,
        center.x + radius, center.y + radius};
    const AABB polyBounds = OffsetBounds(poly.bounds, offset, radius);
    if (!circleBounds.Intersects(polyBounds)) {
        return false;
    }

    bool inside = false;
    const size_t count = poly.points.size();
    for (size_t i = 0, j = count - 1; i < count; j = i++) {
        const Vec2 pi{poly.points[i].x + offset.x, poly.points[i].y + offset.y};
        const Vec2 pj{poly.points[j].x + offset.x, poly.points[j].y + offset.y};
        const bool intersect =
            ((pi.y > center.y) != (pj.y > center.y)) &&
            (center.x < (pj.x - pi.x) * (center.y - pi.y) / (pj.y - pi.y + 0.00001f) + pi.x);
        if (intersect) {
            inside = !inside;
        }
        if (PointSegmentDistanceSquared(center, pj, pi) <= radiusSq) {
            return true;
        }
    }

    return inside;
}

}  // namespace wave
