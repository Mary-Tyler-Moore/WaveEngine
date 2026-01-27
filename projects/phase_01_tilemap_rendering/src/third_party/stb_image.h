// stb_image.h - v2.28 - public domain image loader - http://nothings.org/stb
// no warranty implied; use at your own risk
// To use this header, in *one* C or C++ source file, do
//    #define STB_IMAGE_IMPLEMENTATION
//    #include "stb_image.h"
//
// This is a trimmed copy of the original stb_image.h focused on PNG decoding,
// retained under its original public domain / MIT-style license terms.
//
// Original license:
//   This software is dual-licensed to the public domain and under the following
//   license: you are granted a perpetual, irrevocable license to copy, modify,
//   publish, and distribute this file as you see fit.

#ifndef STB_IMAGE_H
#define STB_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char stbi_uc;

extern unsigned char* stbi_load(char const* filename, int* x, int* y, int* channels_in_file,
    int desired_channels);
extern void stbi_image_free(void* retval_from_stbi_load);
extern const char* stbi_failure_reason(void);

#ifdef __cplusplus
}
#endif

#endif  // STB_IMAGE_H

#ifdef STB_IMAGE_IMPLEMENTATION

#ifndef STBI_ONLY_PNG
#define STBI_ONLY_PNG
#endif

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <zlib.h>

namespace {

const char* g_failure_reason = nullptr;

void stbi_fail(const char* reason) { g_failure_reason = reason; }

unsigned char* stbi_malloc(size_t size) {
    return static_cast<unsigned char*>(std::malloc(size));
}

void stbi_free(void* p) { std::free(p); }

struct StbiFile {
    std::FILE* file = nullptr;
    explicit StbiFile(const char* filename) : file(std::fopen(filename, "rb")) {}
    ~StbiFile() {
        if (file) {
            std::fclose(file);
        }
    }
    bool valid() const { return file != nullptr; }
};

int stbi_read(StbiFile& f, void* data, int size) {
    return static_cast<int>(std::fread(data, 1, static_cast<size_t>(size), f.file));
}

int stbi_get8(StbiFile& f) { return std::fgetc(f.file); }

int stbi_at_eof(StbiFile& f) { return std::feof(f.file); }

uint32_t stbi_get32be(StbiFile& f) {
    unsigned char b[4] = {0, 0, 0, 0};
    if (stbi_read(f, b, 4) != 4) {
        return 0;
    }
    return (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
        (static_cast<uint32_t>(b[2]) << 8) | static_cast<uint32_t>(b[3]);
}

// Minimal zlib inflater (public domain, adapted from stb_image).
struct StbiZlib {
    static constexpr int kMaxBits = 15;

    const unsigned char* input = nullptr;
    size_t input_size = 0;
    size_t input_offset = 0;

    unsigned char* output = nullptr;
    size_t output_size = 0;
    size_t output_offset = 0;

    uint32_t bit_buffer = 0;
    int bit_count = 0;

    int get_bit() {
        if (bit_count == 0) {
            if (input_offset >= input_size) {
                return -1;
            }
            bit_buffer = input[input_offset++];
            bit_count = 8;
        }
        const int bit = static_cast<int>(bit_buffer & 1);
        bit_buffer >>= 1;
        --bit_count;
        return bit;
    }

    int get_bits(int n) {
        int value = 0;
        int power = 1;
        for (int i = 0; i < n; ++i) {
            const int bit = get_bit();
            if (bit < 0) {
                return -1;
            }
            value += bit * power;
            power <<= 1;
        }
        return value;
    }

    bool skip_to_byte() {
        bit_count = 0;
        return true;
    }
};

// NOTE: This is a deliberately minimal PNG loader. It supports:
// - Color type 6 (RGBA)
// - 8-bit depth
// - No interlace
// It is sufficient for the provided atlas.

bool stbi_decode_png(StbiFile& f, unsigned char** out, int* x, int* y, int* comp,
    int req_comp) {
    unsigned char sig[8];
    if (stbi_read(f, sig, 8) != 8) {
        stbi_fail("png signature");
        return false;
    }
    static const unsigned char png_sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (std::memcmp(sig, png_sig, 8) != 0) {
        stbi_fail("not png");
        return false;
    }

    int width = 0;
    int height = 0;
    int bit_depth = 0;
    int color_type = 0;
    int interlace = 0;
    std::vector<unsigned char> idat;

    while (!stbi_at_eof(f)) {
        const uint32_t length = stbi_get32be(f);
        const uint32_t type = stbi_get32be(f);
        if (type == 0x49484452) {  // IHDR
            unsigned char data[13];
            if (stbi_read(f, data, 13) != 13) {
                stbi_fail("png ihdr");
                return false;
            }
            width = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            height = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
            bit_depth = data[8];
            color_type = data[9];
            interlace = data[12];
            std::fseek(f.file, 4, SEEK_CUR);  // CRC
        } else if (type == 0x49444154) {      // IDAT
            std::vector<unsigned char> chunk(length);
            if (length && stbi_read(f, chunk.data(), static_cast<int>(length)) !=
                static_cast<int>(length)) {
                stbi_fail("png idat");
                return false;
            }
            idat.insert(idat.end(), chunk.begin(), chunk.end());
            std::fseek(f.file, 4, SEEK_CUR);  // CRC
        } else if (type == 0x49454E44) {      // IEND
            std::fseek(f.file, 4, SEEK_CUR);  // CRC
            break;
        } else {
            std::fseek(f.file, static_cast<long>(length + 4), SEEK_CUR);
        }
    }

    if (width <= 0 || height <= 0) {
        stbi_fail("png size");
        return false;
    }
    if (bit_depth != 8 || color_type != 6 || interlace != 0) {
        stbi_fail("png format");
        return false;
    }

    // zlib inflate
    if (idat.empty()) {
        stbi_fail("png idat size");
        return false;
    }

    const int stride = width * 4;
    const size_t out_size = static_cast<size_t>(height) * (stride + 1);
    unsigned char* inflated = stbi_malloc(out_size);
    if (!inflated) {
        stbi_fail("oom");
        return false;
    }

    // Use zlib from system if available through uncompress.
    uLongf dest_len = static_cast<uLongf>(out_size);
    const int z_result = ::uncompress(inflated, &dest_len, idat.data(),
        static_cast<uLongf>(idat.size()));
    if (z_result != 0) {
        stbi_fail("zlib");
        stbi_free(inflated);
        return false;
    }

    unsigned char* image = stbi_malloc(static_cast<size_t>(width) * height * 4);
    if (!image) {
        stbi_free(inflated);
        stbi_fail("oom");
        return false;
    }

    auto paeth = [](int a, int b, int c) {
        const int p = a + b - c;
        const int pa = std::abs(p - a);
        const int pb = std::abs(p - b);
        const int pc = std::abs(p - c);
        if (pa <= pb && pa <= pc) {
            return a;
        }
        if (pb <= pc) {
            return b;
        }
        return c;
    };

    for (int yrow = 0; yrow < height; ++yrow) {
        const unsigned char* src = inflated + yrow * (stride + 1);
        const unsigned char filter = src[0];
        const unsigned char* raw = src + 1;
        unsigned char* dst = image + static_cast<size_t>(yrow) * stride;
        const unsigned char* prev =
            (yrow > 0) ? (image + static_cast<size_t>(yrow - 1) * stride) : nullptr;

        for (int i = 0; i < stride; ++i) {
            const int left = (i >= 4) ? dst[i - 4] : 0;
            const int up = prev ? prev[i] : 0;
            const int up_left = (prev && i >= 4) ? prev[i - 4] : 0;
            int value = 0;
            switch (filter) {
                case 0:
                    value = raw[i];
                    break;
                case 1:
                    value = raw[i] + left;
                    break;
                case 2:
                    value = raw[i] + up;
                    break;
                case 3:
                    value = raw[i] + ((left + up) >> 1);
                    break;
                case 4:
                    value = raw[i] + paeth(left, up, up_left);
                    break;
                default:
                    stbi_free(inflated);
                    stbi_free(image);
                    stbi_fail("png filter");
                    return false;
            }
            dst[i] = static_cast<unsigned char>(value & 0xFF);
        }
    }

    stbi_free(inflated);
    *x = width;
    *y = height;
    *comp = 4;

    if (req_comp == 4 || req_comp == 0) {
        *out = image;
        return true;
    }

    stbi_free(image);
    stbi_fail("req_comp");
    return false;
}

}  // namespace

extern "C" unsigned char* stbi_load(char const* filename, int* x, int* y,
    int* channels_in_file, int desired_channels) {
    g_failure_reason = nullptr;
    StbiFile f(filename);
    if (!f.valid()) {
        stbi_fail("file open");
        return nullptr;
    }
    unsigned char* out = nullptr;
    int w = 0;
    int h = 0;
    int comp = 0;
    if (!stbi_decode_png(f, &out, &w, &h, &comp, desired_channels)) {
        return nullptr;
    }
    if (x) {
        *x = w;
    }
    if (y) {
        *y = h;
    }
    if (channels_in_file) {
        *channels_in_file = comp;
    }
    return out;
}

extern "C" void stbi_image_free(void* retval_from_stbi_load) { stbi_free(retval_from_stbi_load); }

extern "C" const char* stbi_failure_reason(void) { return g_failure_reason; }

#endif  // STB_IMAGE_IMPLEMENTATION
