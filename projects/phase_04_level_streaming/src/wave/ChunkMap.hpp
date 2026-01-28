#pragma once

#include <SDL.h>

#include "wave/TilemapLoader.hpp"

namespace wave {

SDL_Rect SrcRectForLocalIndex(const AtlasInfo& atlas, uint32_t localIndex);

int RenderChunk(SDL_Renderer* renderer, SDL_Texture* atlasTexture, const ChunkMap& chunk,
    float cameraX, float cameraY, float screenCenterX, float screenCenterY,
    bool drawBounds);

}  // namespace wave
