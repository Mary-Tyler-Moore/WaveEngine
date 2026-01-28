#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "wave/ChunkId.hpp"

namespace wave {

struct AtlasInfo {
    int firstGid = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    int columns = 0;
    int tileCount = 0;
};

struct ChunkLayer {
    std::string name;
    std::vector<uint32_t> gids;
};

struct ChunkMap {
    ChunkId id;
    int width = 0;
    int height = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    AtlasInfo atlas;
    std::vector<ChunkLayer> layers;
    std::vector<uint8_t> shoreMask;
};

std::optional<ChunkMap> LoadChunkMapFromJson(const std::filesystem::path& path,
    const ChunkId& id, std::string* error);

}  // namespace wave
