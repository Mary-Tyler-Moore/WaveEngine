#include "wave/OverlayText.hpp"

#include <algorithm>

#include "wave/BitmapFont8x8.hpp"

namespace wave {

namespace {

void UnpackRGBA(uint32_t rgba, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a) {
    if (r) {
        *r = static_cast<uint8_t>((rgba >> 24) & 0xFF);
    }
    if (g) {
        *g = static_cast<uint8_t>((rgba >> 16) & 0xFF);
    }
    if (b) {
        *b = static_cast<uint8_t>((rgba >> 8) & 0xFF);
    }
    if (a) {
        *a = static_cast<uint8_t>(rgba & 0xFF);
    }
}

void BlendPixel(uint8_t* pixel, uint8_t srcR, uint8_t srcG, uint8_t srcB, uint8_t srcA) {
    const uint8_t dstR = pixel[0];
    const uint8_t dstG = pixel[1];
    const uint8_t dstB = pixel[2];
    const uint8_t dstA = pixel[3];
    const uint16_t invA = static_cast<uint16_t>(255 - srcA);
    pixel[0] = static_cast<uint8_t>((srcR * srcA + dstR * invA) / 255);
    pixel[1] = static_cast<uint8_t>((srcG * srcA + dstG * invA) / 255);
    pixel[2] = static_cast<uint8_t>((srcB * srcA + dstB * invA) / 255);
    pixel[3] = static_cast<uint8_t>(std::min<int>(255, srcA + dstA));
}

}  // namespace

uint32_t PackRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (static_cast<uint32_t>(r) << 24) | (static_cast<uint32_t>(g) << 16)
        | (static_cast<uint32_t>(b) << 8) | static_cast<uint32_t>(a);
}

void DrawRectRGBA(uint8_t* pixels, int width, int height, int pitch, int x, int y,
    int rectW, int rectH, uint32_t rgba) {
    if (!pixels || width <= 0 || height <= 0 || rectW <= 0 || rectH <= 0) {
        return;
    }
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(width, x + rectW);
    int y1 = std::min(height, y + rectH);
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
    UnpackRGBA(rgba, &r, &g, &b, &a);

    for (int row = y0; row < y1; ++row) {
        uint8_t* scan = pixels + row * pitch + x0 * 4;
        for (int col = x0; col < x1; ++col) {
            BlendPixel(scan, r, g, b, a);
            scan += 4;
        }
    }
}

void DrawTextRGBA(uint8_t* pixels, int width, int height, int pitch, int x, int y,
    std::string_view text, uint32_t rgba, int scale) {
    if (!pixels || text.empty() || scale <= 0) {
        return;
    }

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
    UnpackRGBA(rgba, &r, &g, &b, &a);

    int cursorX = x;
    for (char c : text) {
        const uint8_t* glyph = GlyphForChar(c);
        for (int row = 0; row < kFontHeight; ++row) {
            const uint8_t bits = glyph[row];
            for (int col = 0; col < kFontWidth; ++col) {
                if ((bits & (1u << (7 - col))) == 0) {
                    continue;
                }
                for (int sy = 0; sy < scale; ++sy) {
                    const int pixelY = y + row * scale + sy;
                    if (pixelY < 0 || pixelY >= height) {
                        continue;
                    }
                    for (int sx = 0; sx < scale; ++sx) {
                        const int pixelX = cursorX + col * scale + sx;
                        if (pixelX < 0 || pixelX >= width) {
                            continue;
                        }
                        uint8_t* pixel = pixels + pixelY * pitch + pixelX * 4;
                        BlendPixel(pixel, r, g, b, a);
                    }
                }
            }
        }
        cursorX += (kFontWidth * scale) + scale;
    }
}

}  // namespace wave
