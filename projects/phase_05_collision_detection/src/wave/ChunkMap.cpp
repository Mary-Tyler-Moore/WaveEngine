#include "wave/ChunkMap.hpp"

#include <algorithm>

#include "wave/ChunkId.hpp"

namespace wave {

SDL_Rect SrcRectForLocalIndex(const AtlasInfo& atlas, uint32_t localIndex) {
    const int col = static_cast<int>(localIndex % static_cast<uint32_t>(atlas.columns));
    const int row = static_cast<int>(localIndex / static_cast<uint32_t>(atlas.columns));
    return SDL_Rect{col * atlas.tileWidth, row * atlas.tileHeight, atlas.tileWidth,
        atlas.tileHeight};
}

int RenderChunk(SDL_Renderer* renderer, SDL_Texture* atlasTexture, const ChunkMap& chunk,
    float cameraX, float cameraY, float screenCenterX, float screenCenterY,
    bool drawBounds) {
    if (!atlasTexture || chunk.atlas.tileCount <= 0) {
        return 0;
    }

    const uint32_t atlasFirst = static_cast<uint32_t>(chunk.atlas.firstGid);
    const uint32_t atlasLast =
        atlasFirst + static_cast<uint32_t>(chunk.atlas.tileCount - 1);
    SDL_Rect src{0, 0, chunk.atlas.tileWidth, chunk.atlas.tileHeight};
    SDL_Rect dst{0, 0, chunk.tileWidth, chunk.tileHeight};
    int tilesDrawn = 0;

    for (const ChunkLayer& layer : chunk.layers) {
        const size_t tileCount =
            static_cast<size_t>(chunk.width) * static_cast<size_t>(chunk.height);
        if (layer.gids.size() < tileCount) {
            continue;
        }
        for (size_t index = 0; index < tileCount; ++index) {
            const uint32_t gid = layer.gids[index];
            if (gid == 0) {
                continue;
            }
            if (gid < atlasFirst || gid > atlasLast) {
                continue;
            }
            const uint32_t tileIndex = gid - atlasFirst;
            src = SrcRectForLocalIndex(chunk.atlas, tileIndex);

            const int tileX = static_cast<int>(index % static_cast<size_t>(chunk.width));
            const int tileY = static_cast<int>(index / static_cast<size_t>(chunk.width));
            const float worldX =
                static_cast<float>(chunk.id.originWorldX + tileX * chunk.tileWidth);
            const float worldY =
                static_cast<float>(chunk.id.originWorldY + tileY * chunk.tileHeight);
            dst.x = static_cast<int>((worldX - cameraX) + screenCenterX);
            dst.y = static_cast<int>((worldY - cameraY) + screenCenterY);

            SDL_RenderCopy(renderer, atlasTexture, &src, &dst);
            ++tilesDrawn;
        }
    }

    if (drawBounds) {
        const int chunkPixelW = chunk.width * chunk.tileWidth;
        const int chunkPixelH = chunk.height * chunk.tileHeight;
        SDL_Rect bounds{
            static_cast<int>((static_cast<float>(chunk.id.originWorldX) - cameraX)
                + screenCenterX),
            static_cast<int>((static_cast<float>(chunk.id.originWorldY) - cameraY)
                + screenCenterY),
            chunkPixelW,
            chunkPixelH};
        if (chunk.id.kind == ChunkKind::Water) {
            SDL_SetRenderDrawColor(renderer, 40, 120, 170, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 70, 180, 120, 255);
        }
        SDL_RenderDrawRect(renderer, &bounds);
    }

    return tilesDrawn;
}

}  // namespace wave
