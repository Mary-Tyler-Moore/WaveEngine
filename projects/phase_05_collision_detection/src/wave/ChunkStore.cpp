#include "wave/ChunkStore.hpp"

#include <filesystem>
#include <iostream>

#include "wave/ChunkId.hpp"

namespace wave {

ChunkStore::ChunkStore(std::filesystem::path root, int chunkPixelSize)
    : root_(std::move(root)), chunkPixelSize_(chunkPixelSize) {}

bool ChunkStore::BuildIndex(std::string* error) {
    std::error_code ec;
    if (!std::filesystem::exists(root_, ec)) {
        if (error) {
            *error = "Chunk directory missing: " + root_.string();
        }
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(root_, ec)) {
        if (ec) {
            if (error) {
                *error = "Failed to enumerate chunk directory: " + root_.string();
            }
            return false;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string parseError;
        auto id = ParseChunkFilename(entry.path(), &parseError);
        if (!id) {
            static bool loggedParseError = false;
            if (!loggedParseError) {
                std::cerr << parseError << "\n";
                loggedParseError = true;
            }
            continue;
        }
        const int gridX = id->originWorldX / chunkPixelSize_;
        const int gridY = id->originWorldY / chunkPixelSize_;
        const ChunkKey key{gridX, gridY, id->kind};
        if (index_.find(key) != index_.end()) {
            static bool loggedDuplicate = false;
            if (!loggedDuplicate) {
                std::cerr << "Duplicate chunk key detected for " << entry.path().filename()
                          << "\n";
                loggedDuplicate = true;
            }
            continue;
        }
        ChunkRecord record;
        record.id = *id;
        record.path = entry.path();
        index_.emplace(key, std::move(record));
    }

    if (index_.empty()) {
        if (error) {
            *error = "No valid chunk files found in: " + root_.string();
        }
        return false;
    }

    return true;
}

bool ChunkStore::HasChunk(const ChunkKey& key) const {
    return index_.find(key) != index_.end();
}

std::optional<ChunkRecord> ChunkStore::FindRecord(const ChunkKey& key) const {
    auto it = index_.find(key);
    if (it == index_.end()) {
        return std::nullopt;
    }
    return it->second;
}

const ChunkMap* ChunkStore::GetOrLoad(const ChunkKey& key, int* outLoadCount,
    std::string* error) {
    auto loadedIt = loaded_.find(key);
    if (loadedIt != loaded_.end()) {
        return &loadedIt->second;
    }

    auto record = FindRecord(key);
    if (!record) {
        return nullptr;
    }

    std::string loadError;
    auto map = LoadChunkMapFromJson(record->path, record->id, &loadError);
    if (!map) {
        if (error) {
            *error = loadError;
        }
        return nullptr;
    }

    auto [it, inserted] = loaded_.emplace(key, std::move(*map));
    if (inserted && outLoadCount) {
        ++(*outLoadCount);
    }
    return &it->second;
}

}  // namespace wave
