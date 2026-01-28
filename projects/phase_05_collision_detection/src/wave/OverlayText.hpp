#pragma once

#include <cstdint>
#include <string_view>

namespace wave {

uint32_t PackRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void DrawRectRGBA(uint8_t* pixels, int width, int height, int pitch, int x, int y,
    int rectW, int rectH, uint32_t rgba);

void DrawTextRGBA(uint8_t* pixels, int width, int height, int pitch, int x, int y,
    std::string_view text, uint32_t rgba, int scale);

}  // namespace wave
