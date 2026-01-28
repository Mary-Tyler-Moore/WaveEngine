#include <SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#include "src/third_party/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "src/third_party/stb_image_write.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "wave/ChunkMap.hpp"
#include "wave/ChunkStore.hpp"
#include "wave/OverlayText.hpp"
#include "wave/TelemetryCsv.hpp"
#include "wave/player.hpp"

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr const char* kWindowTitle = "Phase 05 - Collision Detection";
constexpr const char* kChunkDir = "assets/tiles/chunk_non_empty";
constexpr const char* kAtlasPng = "assets/tiles/16k-waves-trans-atlas.png";
constexpr const char* kBoatPng = "assets/boat-64.png";
constexpr const char* kScreenshotPath = "assets/screenshots/phase_05_collision_detection.png";
constexpr const char* kTelemetryPath = "assets/profiles/phase_05_collision_detection.csv";
constexpr SDL_Color kClearColor{0x39, 0x78, 0xA8, 0xFF};
constexpr float kMaxDtSeconds = 0.1f;
constexpr int kBoatSizePixels = 64;
constexpr int kChunkTiles = 25;
constexpr int kChunkPixels = kChunkTiles * 32;
constexpr int kMinActiveRadius = 0;
constexpr int kMaxActiveRadius = 5;
constexpr int kTelemetrySampleMs = 200;

struct SdlGuard {
    explicit SdlGuard(Uint32 flags) : ok_(SDL_Init(flags) == 0) {}
    ~SdlGuard() {
        if (ok_) {
            SDL_Quit();
        }
    }

    SdlGuard(const SdlGuard&) = delete;
    SdlGuard& operator=(const SdlGuard&) = delete;

    bool ok() const { return ok_; }

  private:
    bool ok_ = false;
};

using WindowPtr = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
using RendererPtr = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;
using TexturePtr = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

bool PathExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

std::optional<std::filesystem::path> FindAssetPath(const std::filesystem::path& relativePath) {
    auto tryBase = [&](std::filesystem::path base) -> std::optional<std::filesystem::path> {
        for (int i = 0; i < 6; ++i) {
            const auto candidate = base / relativePath;
            if (PathExists(candidate)) {
                return candidate;
            }
            if (!base.has_parent_path()) {
                break;
            }
            base = base.parent_path();
        }
        return std::nullopt;
    };

    if (auto found = tryBase(std::filesystem::current_path())) {
        return found;
    }

    std::unique_ptr<char, decltype(&SDL_free)> basePath(SDL_GetBasePath(), SDL_free);
    if (basePath) {
        if (auto found = tryBase(std::filesystem::path(basePath.get()))) {
            return found;
        }
    }

    return std::nullopt;
}

TexturePtr LoadTextureFromPng(SDL_Renderer* renderer, const std::filesystem::path& path,
    const char* label, int* outWidth, int* outHeight) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        std::cerr << "Failed to decode " << label << " PNG: " << path.string() << "\n";
        std::cerr << "stb_image failure: " << (stbi_failure_reason() ? stbi_failure_reason() : "")
                  << "\n";
        return TexturePtr(nullptr, SDL_DestroyTexture);
    }

    SDL_Texture* texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width,
            height);
    if (!texture) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << "\n";
        stbi_image_free(pixels);
        return TexturePtr(nullptr, SDL_DestroyTexture);
    }

    const int pitch = width * 4;
    if (SDL_UpdateTexture(texture, nullptr, pixels, pitch) != 0) {
        std::cerr << "SDL_UpdateTexture failed: " << SDL_GetError() << "\n";
        SDL_DestroyTexture(texture);
        stbi_image_free(pixels);
        return TexturePtr(nullptr, SDL_DestroyTexture);
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    stbi_image_free(pixels);

    if (outWidth) {
        *outWidth = width;
    }
    if (outHeight) {
        *outHeight = height;
    }

    return TexturePtr(texture, SDL_DestroyTexture);
}

int ComputeTextWidth(const std::string& text, int scale) {
    constexpr int kGlyphW = 8;
    constexpr int kSpacing = 1;
    if (text.empty()) {
        return 0;
    }
    return static_cast<int>(text.size()) * kGlyphW * scale +
        static_cast<int>(text.size() - 1) * kSpacing * scale;
}

bool SaveScreenshotPNG(SDL_Renderer* renderer, const std::string& path,
    const std::vector<std::string>& overlayLines) {
    int width = 0;
    int height = 0;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0) {
        std::cerr << "SDL_GetRendererOutputSize failed: " << SDL_GetError() << "\n";
        return false;
    }

    std::filesystem::path outputPath(path);
    std::error_code ec;
    std::filesystem::create_directories(outputPath.parent_path(), ec);

    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, pixels.data(),
            width * 4)
        != 0) {
        std::cerr << "SDL_RenderReadPixels failed: " << SDL_GetError() << "\n";
        return false;
    }

    if (!overlayLines.empty()) {
        constexpr int kGlyphW = 8;
        constexpr int kGlyphH = 8;
        constexpr int kLineGap = 2;
        constexpr int kPadding = 4;
        constexpr int kMargin = 8;
        constexpr int kScale = 2;
        int maxWidth = 0;
        for (const auto& line : overlayLines) {
            maxWidth = std::max(maxWidth, ComputeTextWidth(line, kScale));
        }
        const int lineCount = static_cast<int>(overlayLines.size());
        const int textHeight =
            lineCount * kGlyphH * kScale + (lineCount - 1) * kLineGap * kScale;
        const int boxW = maxWidth + kPadding * 2;
        const int boxH = textHeight + kPadding * 2;
        const int boxX = kMargin;
        const int boxY = kMargin;
        wave::DrawRectRGBA(pixels.data(), width, height, width * 4, boxX, boxY, boxW, boxH,
            wave::PackRGBA(0, 0, 0, 180));

        int cursorY = boxY + kPadding;
        for (const auto& line : overlayLines) {
            wave::DrawTextRGBA(pixels.data(), width, height, width * 4, boxX + kPadding,
                cursorY, line, wave::PackRGBA(255, 255, 255, 255), kScale);
            cursorY += (kGlyphH + kLineGap) * kScale;
        }
    }

    const int result = stbi_write_png(outputPath.string().c_str(), width, height, 4,
        pixels.data(), width * 4);
    if (result == 0) {
        std::cerr << "stbi_write_png failed for " << outputPath.string() << "\n";
        return false;
    }

    std::cout << "Saved screenshot: " << outputPath.string() << "\n";
    return true;
}

bool SaveScreenshotPNG(SDL_Renderer* renderer, const std::string& path) {
    std::vector<std::string> empty;
    return SaveScreenshotPNG(renderer, path, empty);
}

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

Vec2 Lerp(const Vec2& a, const Vec2& b, float alpha) {
    return {a.x + (b.x - a.x) * alpha, a.y + (b.y - a.y) * alpha};
}

float Length(const Vec2& v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

struct CameraConfig {
    bool smoothingEnabled = true;
    float followSharpness = 3.0f;
    float followDeadzonePixels = 48.0f;
};

struct Camera {
    Vec2 position;
    CameraConfig config;

    void ResetTo(const Vec2& target) { position = target; }

    void Update(float dtSeconds, const Vec2& target) {
        if (!config.smoothingEnabled) {
            position = target;
            return;
        }
        const Vec2 delta{target.x - position.x, target.y - position.y};
        if (Length(delta) < config.followDeadzonePixels) {
            return;
        }
        const float alpha =
            1.0f - std::exp(-config.followSharpness * std::max(0.0f, dtSeconds));
        position = Lerp(position, target, alpha);
    }
};

Vec2 GetScreenCenter(SDL_Renderer* renderer) {
    int outputW = 0;
    int outputH = 0;
    if (SDL_GetRendererOutputSize(renderer, &outputW, &outputH) != 0) {
        outputW = kWindowWidth;
        outputH = kWindowHeight;
    }
    return {static_cast<float>(outputW) * 0.5f, static_cast<float>(outputH) * 0.5f};
}

int ChunkGridCoord(float worldCoord) {
    return static_cast<int>(std::floor(worldCoord / static_cast<float>(kChunkPixels)));
}

struct AABB {
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    bool Intersects(const AABB& other) const {
        return !(maxX < other.minX || minX > other.maxX || maxY < other.minY || minY > other.maxY);
    }
};

int ClampInt(int value, int minValue, int maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

AABB MakeAABB(const Vec2& center, float halfWidth, float halfHeight) {
    return {center.x - halfWidth, center.y - halfHeight, center.x + halfWidth,
        center.y + halfHeight};
}

struct CollisionQueryResult {
    bool colliding = false;
    int tilesTested = 0;
    int activeChunks = 0;
};

CollisionQueryResult QueryBoatCollision(const std::vector<const wave::ChunkMap*>& islandChunks,
    const AABB& boatBox) {
    CollisionQueryResult result;
    for (const wave::ChunkMap* chunk : islandChunks) {
        if (!chunk || chunk->shoreMask.empty()) {
            continue;
        }
        result.activeChunks += 1;
        const float originX = static_cast<float>(chunk->id.originWorldX);
        const float originY = static_cast<float>(chunk->id.originWorldY);
        const float chunkW = static_cast<float>(chunk->width * chunk->tileWidth);
        const float chunkH = static_cast<float>(chunk->height * chunk->tileHeight);
        const AABB chunkBox{originX, originY, originX + chunkW, originY + chunkH};
        if (!boatBox.Intersects(chunkBox)) {
            continue;
        }

        const float localMinX = (boatBox.minX - originX) / static_cast<float>(chunk->tileWidth);
        const float localMaxX = (boatBox.maxX - originX) / static_cast<float>(chunk->tileWidth);
        const float localMinY = (boatBox.minY - originY) / static_cast<float>(chunk->tileHeight);
        const float localMaxY = (boatBox.maxY - originY) / static_cast<float>(chunk->tileHeight);
        int minX = ClampInt(static_cast<int>(std::floor(localMinX)), 0, chunk->width - 1);
        int maxX = ClampInt(static_cast<int>(std::floor(localMaxX)), 0, chunk->width - 1);
        int minY = ClampInt(static_cast<int>(std::floor(localMinY)), 0, chunk->height - 1);
        int maxY = ClampInt(static_cast<int>(std::floor(localMaxY)), 0, chunk->height - 1);

        for (int ty = minY; ty <= maxY; ++ty) {
            for (int tx = minX; tx <= maxX; ++tx) {
                const size_t index =
                    static_cast<size_t>(ty) * static_cast<size_t>(chunk->width) +
                    static_cast<size_t>(tx);
                result.tilesTested += 1;
                if (index < chunk->shoreMask.size() && chunk->shoreMask[index] != 0) {
                    result.colliding = true;
                    return result;
                }
            }
        }
    }
    return result;
}

void DrawCollisionOverlay(SDL_Renderer* renderer,
    const std::vector<const wave::ChunkMap*>& islandChunks, const AABB& boatBox,
    const Vec2& cameraPos, const Vec2& screenCenter) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 80, 80, 180);
    for (const wave::ChunkMap* chunk : islandChunks) {
        if (!chunk || chunk->shoreMask.empty()) {
            continue;
        }
        for (int ty = 0; ty < chunk->height; ++ty) {
            for (int tx = 0; tx < chunk->width; ++tx) {
                const size_t index =
                    static_cast<size_t>(ty) * static_cast<size_t>(chunk->width) +
                    static_cast<size_t>(tx);
                if (index >= chunk->shoreMask.size() || chunk->shoreMask[index] == 0) {
                    continue;
                }
                const float worldX =
                    static_cast<float>(chunk->id.originWorldX + tx * chunk->tileWidth);
                const float worldY =
                    static_cast<float>(chunk->id.originWorldY + ty * chunk->tileHeight);
                SDL_Rect rect{
                    static_cast<int>((worldX - cameraPos.x) + screenCenter.x),
                    static_cast<int>((worldY - cameraPos.y) + screenCenter.y),
                    chunk->tileWidth,
                    chunk->tileHeight};
                SDL_RenderDrawRect(renderer, &rect);
            }
        }
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    SDL_Rect boatRect{
        static_cast<int>((boatBox.minX - cameraPos.x) + screenCenter.x),
        static_cast<int>((boatBox.minY - cameraPos.y) + screenCenter.y),
        static_cast<int>(boatBox.maxX - boatBox.minX),
        static_cast<int>(boatBox.maxY - boatBox.minY)};
    SDL_RenderDrawRect(renderer, &boatRect);
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    SdlGuard sdl(SDL_INIT_VIDEO);
    if (!sdl.ok()) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    WindowPtr window(SDL_CreateWindow(
                         kWindowTitle,
                         SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED,
                         kWindowWidth,
                         kWindowHeight,
                         SDL_WINDOW_ALLOW_HIGHDPI),
        SDL_DestroyWindow);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return 1;
    }

    RendererPtr renderer(SDL_CreateRenderer(
                             window.get(),
                             -1,
                             SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
        SDL_DestroyRenderer);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    const auto chunkDirPath = FindAssetPath(kChunkDir);
    if (!chunkDirPath) {
        std::cerr << "Missing chunk directory at " << kChunkDir << "\n";
        return 1;
    }

    wave::ChunkStore chunkStore(*chunkDirPath, kChunkPixels);
    std::string chunkIndexError;
    if (!chunkStore.BuildIndex(&chunkIndexError)) {
        std::cerr << chunkIndexError << "\n";
        return 1;
    }

    const auto atlasPath = FindAssetPath(kAtlasPng);
    if (!atlasPath) {
        std::cerr << "Missing atlas PNG at " << kAtlasPng << "\n";
        return 1;
    }

    int atlasWidth = 0;
    int atlasHeight = 0;
    TexturePtr atlasTexture = LoadTextureFromPng(renderer.get(), *atlasPath, "Atlas", &atlasWidth,
        &atlasHeight);
    if (!atlasTexture) {
        return 1;
    }
    (void)atlasWidth;
    (void)atlasHeight;

    const auto boatPath = FindAssetPath(kBoatPng);
    if (!boatPath) {
        std::cerr << "Missing boat PNG at " << kBoatPng << "\n";
        return 1;
    }

    int boatWidth = 0;
    int boatHeight = 0;
    TexturePtr boatTexture = LoadTextureFromPng(renderer.get(), *boatPath, "Boat", &boatWidth,
        &boatHeight);
    if (!boatTexture) {
        return 1;
    }
    (void)boatWidth;
    (void)boatHeight;

    Player player;
    player.x = 500.0f;
    player.y = 0.0f;
    Camera camera;
    camera.ResetTo({player.x, player.y});

    const bool cullingEnabled = true;
    bool running = true;
    bool drawChunkBounds = false;
    bool drawCollisionDebug = false;
    int activeRadius = 1;
    int lastRadius = activeRadius;
    bool requestScreenshot = false;
    std::vector<std::string> pendingScreenshotLines;

    using Clock = std::chrono::steady_clock;
    auto lastFrameTime = Clock::now();
    auto perfWindowStart = lastFrameTime;
    const auto startTime = lastFrameTime;
    auto nextSampleTime = lastFrameTime + std::chrono::milliseconds(kTelemetrySampleMs);
    int perfFrameCount = 0;
    int perfTilesSum = 0;
    int perfActiveSum = 0;
    int perfChunkLoads = 0;
    float perfUpdateTime = 0.0f;
    float perfSelectTime = 0.0f;
    float perfWaterTime = 0.0f;
    float perfIslandTime = 0.0f;
    float perfPresentTime = 0.0f;
    float lastFps = 0.0f;
    float lastAvgMs = 0.0f;
    int lastAvgTiles = 0;
    int lastAvgActive = 0;
    int lastCollisionTilesTested = 0;
    int lastActiveCollisionChunks = 0;
    bool lastColliding = false;
    int frameCounter = 0;

    wave::TelemetryCsv telemetry(kTelemetryPath);

    while (running) {
        const auto frameStart = Clock::now();
        std::chrono::duration<float> delta = frameStart - lastFrameTime;
        lastFrameTime = frameStart;
        const float dt = std::min(delta.count(), kMaxDtSeconds);
        const float dtMs = dt * 1000.0f;

        SDL_Event event{};
        while (SDL_PollEvent(&event) == 1) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.repeat != 0) {
                    continue;
                }
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_F12) {
                    const float uptimeSeconds =
                        std::chrono::duration<float>(frameStart - startTime).count();
                    std::vector<std::string> lines;
                    char line[128];
                    lines.emplace_back("Phase 05 - Collision Detection");
                    std::snprintf(line, sizeof(line), "FPS %.1f | ms %.2f | tiles %d",
                        lastFps, lastAvgMs, lastAvgTiles);
                    lines.emplace_back(line);
                    std::snprintf(line, sizeof(line), "Cull %s | r %d | loaded %d | vis %d",
                        cullingEnabled ? "ON" : "OFF",
                        activeRadius,
                        chunkStore.loadedCount(),
                        lastAvgActive);
                    lines.emplace_back(line);
                    std::snprintf(line, sizeof(line), "Boat %.1f, %.1f",
                        player.x, player.y);
                    lines.emplace_back(line);
                    std::snprintf(line, sizeof(line), "Collision %s | chunks %d | tested %d",
                        lastColliding ? "SHORE" : "CLEAR",
                        lastActiveCollisionChunks,
                        lastCollisionTilesTested);
                    lines.emplace_back(line);
                    std::snprintf(line, sizeof(line), "Uptime %.1fs", uptimeSeconds);
                    lines.emplace_back(line);
                    pendingScreenshotLines = std::move(lines);
                    requestScreenshot = true;
                } else if (event.key.keysym.sym == SDLK_r) {
                    camera.ResetTo({player.x, player.y});
                } else if (event.key.keysym.sym == SDLK_c) {
                    camera.config.smoothingEnabled = !camera.config.smoothingEnabled;
                } else if (event.key.keysym.sym == SDLK_LEFTBRACKET) {
                    activeRadius = std::max(kMinActiveRadius, activeRadius - 1);
                } else if (event.key.keysym.sym == SDLK_RIGHTBRACKET) {
                    activeRadius = std::min(kMaxActiveRadius, activeRadius + 1);
                } else if (event.key.keysym.sym == SDLK_v) {
                    drawChunkBounds = !drawChunkBounds;
                } else if (event.key.keysym.sym == SDLK_b) {
                    drawCollisionDebug = !drawCollisionDebug;
                }
            }
        }
        if (activeRadius != lastRadius) {
            wave::TelemetrySample eventRow{};
            eventRow.t_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(frameStart - startTime)
                    .count();
            eventRow.frame = frameCounter;
            eventRow.dt_ms = dtMs;
            eventRow.fps_smooth = lastFps;
            eventRow.boat_x = player.x;
            eventRow.boat_y = player.y;
            eventRow.cam_x = camera.position.x;
            eventRow.cam_y = camera.position.y;
            eventRow.culling_enabled = cullingEnabled ? 1 : 0;
            eventRow.radius_tiles = activeRadius * kChunkTiles;
            eventRow.chunks_visible = 0;
            eventRow.chunks_loaded_total = chunkStore.loadedCount();
            telemetry.Event(eventRow, "radius_change");
            lastRadius = activeRadius;
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        PlayerInput input;
        if (keys[SDL_SCANCODE_W]) {
            input.moveY -= 1.0f;
        }
        if (keys[SDL_SCANCODE_S]) {
            input.moveY += 1.0f;
        }
        if (keys[SDL_SCANCODE_A]) {
            input.moveX -= 1.0f;
        }
        if (keys[SDL_SCANCODE_D]) {
            input.moveX += 1.0f;
        }
        const float magnitude = std::sqrt(input.moveX * input.moveX + input.moveY * input.moveY);
        if (magnitude > 0.0f) {
            input.moveX /= magnitude;
            input.moveY /= magnitude;
        }
        const Vec2 desiredDelta{
            input.moveX * player.config.speedPixelsPerSecond * dt,
            input.moveY * player.config.speedPixelsPerSecond * dt};
        player.x += desiredDelta.x;
        player.y += desiredDelta.y;
        camera.Update(dt, {player.x, player.y});

        const auto afterUpdate = Clock::now();

        const int chunkGX = ChunkGridCoord(camera.position.x);
        const int chunkGY = ChunkGridCoord(camera.position.y);
        std::vector<const wave::ChunkMap*> waterChunks;
        std::vector<const wave::ChunkMap*> islandChunks;
        int activeChunksThisFrame = 0;
        int loadsThisFrame = 0;
        for (int gy = chunkGY - activeRadius; gy <= chunkGY + activeRadius; ++gy) {
            for (int gx = chunkGX - activeRadius; gx <= chunkGX + activeRadius; ++gx) {
                for (wave::ChunkKind kind : {wave::ChunkKind::Water, wave::ChunkKind::Island}) {
                    wave::ChunkKey key{gx, gy, kind};
                    if (!chunkStore.HasChunk(key)) {
                        continue;
                    }
                    std::string loadError;
                    const wave::ChunkMap* chunk =
                        chunkStore.GetOrLoad(key, &loadsThisFrame, &loadError);
                    if (!chunk) {
                        if (!loadError.empty()) {
                            std::cerr << loadError << "\n";
                        }
                        continue;
                    }
                    if (kind == wave::ChunkKind::Water) {
                        waterChunks.push_back(chunk);
                    } else {
                        islandChunks.push_back(chunk);
                    }
                }
            }
        }
        activeChunksThisFrame =
            static_cast<int>(waterChunks.size() + islandChunks.size());
        perfChunkLoads += loadsThisFrame;

        const float boatHalfWidth = static_cast<float>(kBoatSizePixels) * 0.25f;
        const float boatHalfHeight = static_cast<float>(kBoatSizePixels) * 0.5f;
        const AABB boatBox = MakeAABB({player.x, player.y}, boatHalfWidth, boatHalfHeight);
        const CollisionQueryResult collisionQuery = QueryBoatCollision(islandChunks, boatBox);
        const bool isColliding = collisionQuery.colliding;
        const int collisionTilesTested = collisionQuery.tilesTested;
        const int activeCollisionChunks = collisionQuery.activeChunks;
        lastCollisionTilesTested = collisionTilesTested;
        lastActiveCollisionChunks = activeCollisionChunks;
        lastColliding = isColliding;

        const auto afterSelect = Clock::now();

        SDL_SetRenderDrawColor(renderer.get(), kClearColor.r, kClearColor.g, kClearColor.b,
            kClearColor.a);
        SDL_RenderClear(renderer.get());

        const Vec2 screenCenter = GetScreenCenter(renderer.get());
        int tilesDrawnThisFrame = 0;
        for (const wave::ChunkMap* chunk : waterChunks) {
            tilesDrawnThisFrame += wave::RenderChunk(renderer.get(), atlasTexture.get(), *chunk,
                camera.position.x, camera.position.y, screenCenter.x, screenCenter.y,
                drawChunkBounds);
        }
        const auto afterWater = Clock::now();

        for (const wave::ChunkMap* chunk : islandChunks) {
            tilesDrawnThisFrame += wave::RenderChunk(renderer.get(), atlasTexture.get(), *chunk,
                camera.position.x, camera.position.y, screenCenter.x, screenCenter.y,
                drawChunkBounds);
        }
        const auto afterIsland = Clock::now();

        if (drawCollisionDebug || requestScreenshot) {
            DrawCollisionOverlay(renderer.get(), islandChunks, boatBox,
                {camera.position.x, camera.position.y}, screenCenter);
        }

        const float boatScreenX = (player.x - camera.position.x) + screenCenter.x;
        const float boatScreenY = (player.y - camera.position.y) + screenCenter.y;
        SDL_Rect boatDst{
            static_cast<int>(boatScreenX - static_cast<float>(kBoatSizePixels) * 0.5f),
            static_cast<int>(boatScreenY - static_cast<float>(kBoatSizePixels) * 0.5f),
            kBoatSizePixels,
            kBoatSizePixels};
        SDL_RenderCopy(renderer.get(), boatTexture.get(), nullptr, &boatDst);

        if (requestScreenshot) {
            SaveScreenshotPNG(renderer.get(), kScreenshotPath, pendingScreenshotLines);
            requestScreenshot = false;
            pendingScreenshotLines.clear();
        }

        SDL_RenderPresent(renderer.get());
        const auto afterPresent = Clock::now();

        if (loadsThisFrame > 0) {
            wave::TelemetrySample loadEvent{};
            loadEvent.t_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(frameStart - startTime)
                    .count();
            loadEvent.frame = frameCounter;
            loadEvent.dt_ms = dtMs;
            loadEvent.fps_smooth = lastFps;
            loadEvent.boat_x = player.x;
            loadEvent.boat_y = player.y;
            loadEvent.cam_x = camera.position.x;
            loadEvent.cam_y = camera.position.y;
            loadEvent.culling_enabled = cullingEnabled ? 1 : 0;
            loadEvent.radius_tiles = activeRadius * kChunkTiles;
            loadEvent.chunks_visible = activeChunksThisFrame;
            loadEvent.chunks_loaded_total = chunkStore.loadedCount();
            loadEvent.chunks_load_started = loadsThisFrame;
            loadEvent.chunks_load_finished = loadsThisFrame;
            telemetry.Event(loadEvent, "chunk_loads");
        }

        perfFrameCount += 1;
        perfTilesSum += tilesDrawnThisFrame;
        perfActiveSum += activeChunksThisFrame;
        perfUpdateTime += std::chrono::duration<float>(afterUpdate - frameStart).count();
        perfSelectTime += std::chrono::duration<float>(afterSelect - afterUpdate).count();
        perfWaterTime += std::chrono::duration<float>(afterWater - afterSelect).count();
        perfIslandTime += std::chrono::duration<float>(afterIsland - afterWater).count();
        perfPresentTime += std::chrono::duration<float>(afterPresent - afterIsland).count();

        std::chrono::duration<float> perfElapsed = frameStart - perfWindowStart;
        if (perfElapsed.count() >= 1.0f) {
            const float seconds = perfElapsed.count();
            const float fps = static_cast<float>(perfFrameCount) / seconds;
            const float avgMs = (seconds / std::max(1, perfFrameCount)) * 1000.0f;
            const int avgTiles =
                (perfFrameCount > 0) ? (perfTilesSum / perfFrameCount) : 0;
            const int avgActive =
                (perfFrameCount > 0) ? (perfActiveSum / perfFrameCount) : 0;
            const float avgUpdateMs =
                (perfFrameCount > 0) ? (perfUpdateTime / perfFrameCount) * 1000.0f : 0.0f;
            const float avgSelectMs =
                (perfFrameCount > 0) ? (perfSelectTime / perfFrameCount) * 1000.0f : 0.0f;
            const float avgWaterMs =
                (perfFrameCount > 0) ? (perfWaterTime / perfFrameCount) * 1000.0f : 0.0f;
            const float avgIslandMs =
                (perfFrameCount > 0) ? (perfIslandTime / perfFrameCount) * 1000.0f : 0.0f;
            const float avgPresentMs =
                (perfFrameCount > 0) ? (perfPresentTime / perfFrameCount) * 1000.0f : 0.0f;

            char title[256];
            std::snprintf(title, sizeof(title),
                "%s | %.1f FPS | %.2f ms | r %d | loaded %d | active %d | tiles %d | loads/s %d"
                " | u %.2f s %.2f w %.2f i %.2f p %.2f",
                kWindowTitle,
                fps,
                avgMs,
                activeRadius,
                chunkStore.loadedCount(),
                avgActive,
                avgTiles,
                perfChunkLoads,
                avgUpdateMs,
                avgSelectMs,
                avgWaterMs,
                avgIslandMs,
                avgPresentMs);
            SDL_SetWindowTitle(window.get(), title);

            std::cout << "Collision: chunks=" << lastActiveCollisionChunks
                      << " tested=" << lastCollisionTilesTested
                      << " state=" << (lastColliding ? "SHORE" : "CLEAR") << "\n";

            lastFps = fps;
            lastAvgMs = avgMs;
            lastAvgTiles = avgTiles;
            lastAvgActive = avgActive;
            perfWindowStart = frameStart;
            perfFrameCount = 0;
            perfTilesSum = 0;
            perfActiveSum = 0;
            perfChunkLoads = 0;
            perfUpdateTime = 0.0f;
            perfSelectTime = 0.0f;
            perfWaterTime = 0.0f;
            perfIslandTime = 0.0f;
            perfPresentTime = 0.0f;
        }

        if (frameStart >= nextSampleTime) {
            wave::TelemetrySample sample{};
            sample.t_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(frameStart - startTime)
                    .count();
            sample.frame = frameCounter;
            sample.dt_ms = dtMs;
            sample.fps_smooth = lastFps;
            sample.boat_x = player.x;
            sample.boat_y = player.y;
            sample.cam_x = camera.position.x;
            sample.cam_y = camera.position.y;
            sample.culling_enabled = cullingEnabled ? 1 : 0;
            sample.radius_tiles = activeRadius * kChunkTiles;
            sample.chunks_visible = activeChunksThisFrame;
            sample.chunks_loaded_total = chunkStore.loadedCount();
            sample.chunks_load_started = loadsThisFrame;
            sample.chunks_load_finished = loadsThisFrame;
            telemetry.Sample(sample);
            nextSampleTime = frameStart + std::chrono::milliseconds(kTelemetrySampleMs);
        }
        frameCounter += 1;
    }

    return 0;
}
