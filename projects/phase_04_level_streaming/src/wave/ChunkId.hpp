#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace wave {

enum class ChunkKind {
    Water,
    Island,
};

struct ChunkId {
    ChunkKind kind = ChunkKind::Water;
    int originTileX = 0;
    int originTileY = 0;
    int originWorldX = 0;
    int originWorldY = 0;
    std::string filename;
};

struct ChunkKey {
    int gridX = 0;
    int gridY = 0;
    ChunkKind kind = ChunkKind::Water;

    bool operator==(const ChunkKey& other) const {
        return gridX == other.gridX && gridY == other.gridY && kind == other.kind;
    }
};

struct ChunkKeyHash {
    size_t operator()(const ChunkKey& key) const {
        size_t h1 = std::hash<int>{}(key.gridX);
        size_t h2 = std::hash<int>{}(key.gridY);
        size_t h3 = std::hash<int>{}(static_cast<int>(key.kind));
        return (h1 * 1315423911u) ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2)) ^ h3;
    }
};

std::optional<ChunkId> ParseChunkFilename(const std::filesystem::path& path,
    std::string* error);

}  // namespace wave
