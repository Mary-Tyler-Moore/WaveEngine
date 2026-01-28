#include "wave/ChunkId.hpp"

#include <cerrno>
#include <cstdlib>
#include <vector>

namespace wave {

namespace {

bool ParseInt(const std::string& text, int* out) {
    if (!out) {
        return false;
    }
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    long value = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
        return false;
    }
    *out = static_cast<int>(value);
    return true;
}

std::vector<std::string> Split(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : text) {
        if (c == delimiter) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(current);
    return parts;
}

}  // namespace

std::optional<ChunkId> ParseChunkFilename(const std::filesystem::path& path,
    std::string* error) {
    const std::string name = path.filename().string();
    constexpr const char* kWaterPrefix = "water_chunk_";
    constexpr const char* kIslandPrefix = "island_chunk_";

    ChunkId id;
    std::string remainder;
    if (name.rfind(kWaterPrefix, 0) == 0) {
        id.kind = ChunkKind::Water;
        remainder = name.substr(std::string(kWaterPrefix).size());
    } else if (name.rfind(kIslandPrefix, 0) == 0) {
        id.kind = ChunkKind::Island;
        remainder = name.substr(std::string(kIslandPrefix).size());
    } else {
        if (error) {
            *error = "Chunk filename missing expected prefix: " + name;
        }
        return std::nullopt;
    }

    const std::vector<std::string> parts = Split(remainder, '_');
    if (parts.size() != 4) {
        if (error) {
            *error = "Chunk filename has unexpected format: " + name;
        }
        return std::nullopt;
    }

    if (!ParseInt(parts[0], &id.originTileX) || !ParseInt(parts[1], &id.originTileY)
        || !ParseInt(parts[2], &id.originWorldX) || !ParseInt(parts[3], &id.originWorldY)) {
        if (error) {
            *error = "Chunk filename has non-numeric coordinate: " + name;
        }
        return std::nullopt;
    }

    id.filename = name;
    return id;
}

}  // namespace wave
