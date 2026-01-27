// Minimal PNG writer interface compatible with stb_image_write.h usage.
// Implements stbi_write_png using zlib for compression.
// Public domain / MIT-style license compatible with stb_image_write expectations.

#ifndef STB_IMAGE_WRITE_H
#define STB_IMAGE_WRITE_H

#ifdef __cplusplus
extern "C" {
#endif

extern int stbi_write_png(char const* filename, int w, int h, int comp, const void* data,
    int stride_in_bytes);

#ifdef __cplusplus
}
#endif

#endif  // STB_IMAGE_WRITE_H

#ifdef STB_IMAGE_WRITE_IMPLEMENTATION

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <zlib.h>

namespace {

void WriteUInt32BE(std::FILE* file, uint32_t value) {
    unsigned char bytes[4] = {
        static_cast<unsigned char>((value >> 24) & 0xFF),
        static_cast<unsigned char>((value >> 16) & 0xFF),
        static_cast<unsigned char>((value >> 8) & 0xFF),
        static_cast<unsigned char>(value & 0xFF),
    };
    std::fwrite(bytes, 1, 4, file);
}

uint32_t ChunkCrc(const char type[4], const unsigned char* data, size_t length) {
    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const unsigned char*>(type), 4);
    if (data && length > 0) {
        crc = crc32(crc, data, static_cast<uInt>(length));
    }
    return crc;
}

bool WriteChunk(std::FILE* file, const char type[4], const unsigned char* data, size_t length) {
    WriteUInt32BE(file, static_cast<uint32_t>(length));
    std::fwrite(type, 1, 4, file);
    if (data && length > 0) {
        std::fwrite(data, 1, length, file);
    }
    const uint32_t crc = ChunkCrc(type, data, length);
    WriteUInt32BE(file, crc);
    return std::ferror(file) == 0;
}

}  // namespace

extern "C" int stbi_write_png(char const* filename, int w, int h, int comp, const void* data,
    int stride_in_bytes) {
    if (comp != 4 || w <= 0 || h <= 0) {
        return 0;
    }
    std::FILE* file = std::fopen(filename, "wb");
    if (!file) {
        return 0;
    }

    const unsigned char signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    std::fwrite(signature, 1, 8, file);

    unsigned char ihdr[13] = {
        static_cast<unsigned char>((w >> 24) & 0xFF),
        static_cast<unsigned char>((w >> 16) & 0xFF),
        static_cast<unsigned char>((w >> 8) & 0xFF),
        static_cast<unsigned char>(w & 0xFF),
        static_cast<unsigned char>((h >> 24) & 0xFF),
        static_cast<unsigned char>((h >> 16) & 0xFF),
        static_cast<unsigned char>((h >> 8) & 0xFF),
        static_cast<unsigned char>(h & 0xFF),
        8,   // bit depth
        6,   // color type RGBA
        0,   // compression
        0,   // filter
        0    // interlace
    };
    if (!WriteChunk(file, "IHDR", ihdr, sizeof(ihdr))) {
        std::fclose(file);
        return 0;
    }

    const int row_bytes = w * 4;
    const int stride = stride_in_bytes > 0 ? stride_in_bytes : row_bytes;
    std::vector<unsigned char> scanlines(static_cast<size_t>(h) * (row_bytes + 1));
    const unsigned char* src = static_cast<const unsigned char*>(data);
    for (int y = 0; y < h; ++y) {
        unsigned char* dst = scanlines.data() + static_cast<size_t>(y) * (row_bytes + 1);
        dst[0] = 0;  // filter type 0
        std::memcpy(dst + 1, src + static_cast<size_t>(y) * stride, row_bytes);
    }

    uLongf compressed_size = compressBound(static_cast<uLong>(scanlines.size()));
    std::vector<unsigned char> compressed(compressed_size);
    const int z_result = compress2(compressed.data(), &compressed_size, scanlines.data(),
        static_cast<uLong>(scanlines.size()), Z_BEST_SPEED);
    if (z_result != Z_OK) {
        std::fclose(file);
        return 0;
    }
    compressed.resize(static_cast<size_t>(compressed_size));

    if (!WriteChunk(file, "IDAT", compressed.data(), compressed.size())) {
        std::fclose(file);
        return 0;
    }

    if (!WriteChunk(file, "IEND", nullptr, 0)) {
        std::fclose(file);
        return 0;
    }

    const int ok = std::ferror(file) == 0;
    std::fclose(file);
    return ok ? 1 : 0;
}

#endif  // STB_IMAGE_WRITE_IMPLEMENTATION
