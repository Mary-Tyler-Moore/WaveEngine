#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wave {

struct AABB {
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    bool Intersects(const AABB& other) const {
        return !(maxX < other.minX || minX > other.maxX || maxY < other.minY || minY > other.maxY);
    }
};

struct CollisionRect {
    AABB bounds;
};

struct CollisionChunk {
    int originWorldX = 0;
    int originWorldY = 0;
    int width = 0;
    int height = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    std::vector<CollisionRect> rects;
};

struct CollisionChunkKey {
    int gridX = 0;
    int gridY = 0;

    bool operator==(const CollisionChunkKey& other) const {
        return gridX == other.gridX && gridY == other.gridY;
    }
};

struct CollisionChunkKeyHash {
    size_t operator()(const CollisionChunkKey& key) const {
        size_t h1 = std::hash<int>{}(key.gridX);
        size_t h2 = std::hash<int>{}(key.gridY);
        return (h1 * 1315423911u) ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
    }
};

struct CollisionChunkInfo {
    CollisionChunkKey key;
    int originWorldX = 0;
    int originWorldY = 0;
    std::filesystem::path path;
};

class CollisionStore {
  public:
    CollisionStore(std::filesystem::path root, int chunkPixels, int chunkTiles);

    bool BuildIndex(const std::filesystem::path& instancesPath,
        const std::filesystem::path& objectsPath, std::string* error);

    bool HasChunk(const CollisionChunkKey& key) const;

    const CollisionChunk* GetOrLoad(const CollisionChunkKey& key, int* outLoads,
        std::string* error);

    int indexedCount() const { return static_cast<int>(index_.size()); }
    int loadedCount() const { return static_cast<int>(loaded_.size()); }

  private:
    std::optional<CollisionChunkInfo> FindChunk(const CollisionChunkKey& key) const;

    std::filesystem::path root_;
    int chunkPixels_ = 0;
    int chunkTiles_ = 0;
    std::unordered_map<CollisionChunkKey, CollisionChunkInfo, CollisionChunkKeyHash> index_;
    std::unordered_map<CollisionChunkKey, CollisionChunk, CollisionChunkKeyHash> loaded_;
    std::unordered_set<std::string> objectNames_;
};

}  // namespace wave
