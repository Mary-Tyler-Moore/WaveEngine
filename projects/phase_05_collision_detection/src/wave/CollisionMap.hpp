#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace wave {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct AABB {
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    bool Intersects(const AABB& other) const {
        return !(maxX < other.minX || minX > other.maxX || maxY < other.minY || minY > other.maxY);
    }
};

struct Polygon {
    std::vector<Vec2> points;
    AABB bounds;
};

struct CollisionMap {
    int width = 0;
    int height = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    int originWorldX = 0;
    int originWorldY = 0;
    int tilesetFirstGid = 0;
    std::unordered_map<int, std::vector<Polygon>> tilePolygons;
    std::vector<std::vector<int>> cellTiles;
};

std::optional<CollisionMap> LoadCollisionMapFromJson(const std::filesystem::path& path,
    std::string* error);

float PointSegmentDistanceSquared(const Vec2& p, const Vec2& a, const Vec2& b);

bool CirclePolygonIntersect(const Vec2& center, float radius, const Polygon& poly,
    const Vec2& offset);

}  // namespace wave
