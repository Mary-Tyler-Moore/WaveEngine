#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

#include "wave/ChunkId.hpp"
#include "wave/TilemapLoader.hpp"

namespace wave {

struct ChunkRecord {
    ChunkId id;
    std::filesystem::path path;
};

class ChunkStore {
  public:
    ChunkStore(std::filesystem::path root, int chunkPixelSize);

    bool BuildIndex(std::string* error);

    bool HasChunk(const ChunkKey& key) const;

    const ChunkMap* GetOrLoad(const ChunkKey& key, int* outLoadCount,
        std::string* error);

    int loadedCount() const { return static_cast<int>(loaded_.size()); }
    int indexedCount() const { return static_cast<int>(index_.size()); }

  private:
    std::optional<ChunkRecord> FindRecord(const ChunkKey& key) const;

    std::filesystem::path root_;
    int chunkPixelSize_ = 0;
    std::unordered_map<ChunkKey, ChunkRecord, ChunkKeyHash> index_;
    std::unordered_map<ChunkKey, ChunkMap, ChunkKeyHash> loaded_;
};

}  // namespace wave
