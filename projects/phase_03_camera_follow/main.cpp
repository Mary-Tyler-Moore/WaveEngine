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

#include "tilemap.hpp"
#include "wave/player.hpp"

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr const char* kWindowTitle = "Phase 03 - Camera Follow";
constexpr const char* kTilemapJson = "assets/tiles/ww-16k-all-islands.json";
constexpr const char* kAtlasPng = "assets/tiles/16k-waves-trans-atlas.png";
constexpr const char* kBoatPng = "assets/boat-64.png";
constexpr const char* kScreenshotPath = "assets/screenshots/phase_03_camera_follow.png";
constexpr SDL_Color kClearColor{0x39, 0x78, 0xA8, 0xFF};
constexpr float kMaxDtSeconds = 0.1f;
constexpr int kBoatSizePixels = 64;

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
    if (kDebugOverlay) {
        std::cout << label << " decode: " << path.string() << " size=" << width << "x" << height
                  << " channels_in_file=" << channels << " forced_channels=4\n";
    }

    SDL_Texture* texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width,
            height);
    if (!texture) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << "\n";
        stbi_image_free(pixels);
        return TexturePtr(nullptr, SDL_DestroyTexture);
    }
    Uint32 format = 0;
    int access = 0;
    int texW = 0;
    int texH = 0;
    if (SDL_QueryTexture(texture, &format, &access, &texW, &texH) == 0) {
        if (kDebugOverlay) {
            std::cout << label << " texture: format=" << SDL_GetPixelFormatName(format)
                      << " access=" << access << " size=" << texW << "x" << texH << "\n";
        }
    } else {
        std::cerr << "SDL_QueryTexture failed: " << SDL_GetError() << "\n";
    }

    const int pitch = width * 4;
    if (kDebugOverlay) {
        std::cout << label << " upload: pitch=" << pitch << " bytes_per_pixel=4\n";
    }
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

bool SaveScreenshotPNG(SDL_Renderer* renderer, const std::string& path) {
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

    const int result = stbi_write_png(outputPath.string().c_str(), width, height, 4,
        pixels.data(), width * 4);
    if (result == 0) {
        std::cerr << "stbi_write_png failed for " << outputPath.string() << "\n";
        return false;
    }

    std::cout << "Saved screenshot: " << outputPath.string() << "\n";
    return true;
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

Vec2 GetScreenCenter(SDL_Renderer* renderer, const Tilemap& map) {
    int outputW = 0;
    int outputH = 0;
    if (SDL_GetRendererOutputSize(renderer, &outputW, &outputH) != 0) {
        outputW = map.width * map.tileWidth;
        outputH = map.height * map.tileHeight;
    }
    return {static_cast<float>(outputW) * 0.5f, static_cast<float>(outputH) * 0.5f};
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

    const auto tilemapPath = FindAssetPath(kTilemapJson);
    if (!tilemapPath) {
        std::cerr << "Missing tilemap JSON at " << kTilemapJson << "\n";
        return 1;
    }

    std::string tilemapError;
    auto tilemap = LoadTilemapFromJson(*tilemapPath, &tilemapError);
    if (!tilemap) {
        std::cerr << tilemapError << "\n";
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
    if (kDebugOverlay) {
        if (SDL_SetTextureBlendMode(atlasTexture.get(), SDL_BLENDMODE_NONE) == 0) {
            std::cout << "Atlas blend mode set to NONE (debug)\n";
        } else {
            std::cerr << "SDL_SetTextureBlendMode failed: " << SDL_GetError() << "\n";
        }
    }

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

    Player player;
    player.x = static_cast<float>(tilemap->width * tilemap->tileWidth) * 0.5f;
    player.y = static_cast<float>(tilemap->height * tilemap->tileHeight) * 0.5f;
    Camera camera;
    camera.ResetTo({player.x, player.y});
    bool running = true;
    uint32_t lastTilesLog = SDL_GetTicks();
    using Clock = std::chrono::steady_clock;
    auto lastFrameTime = Clock::now();
    auto perfWindowStart = lastFrameTime;
    int perfFrameCount = 0;
    int perfTilesSum = 0;
    while (running) {
        const auto frameTime = Clock::now();
        std::chrono::duration<float> delta = frameTime - lastFrameTime;
        lastFrameTime = frameTime;
        const float dt = std::min(delta.count(), kMaxDtSeconds);

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
                    SaveScreenshotPNG(renderer.get(), kScreenshotPath);
                } else if (event.key.keysym.sym == SDLK_r) {
                    camera.ResetTo({player.x, player.y});
                } else if (event.key.keysym.sym == SDLK_c) {
                    camera.config.smoothingEnabled = !camera.config.smoothingEnabled;
                    std::cout << "Camera smoothing "
                              << (camera.config.smoothingEnabled ? "enabled" : "disabled")
                              << "\n";
                }
            }
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
        player.Update(dt, input);
        camera.Update(dt, {player.x, player.y});

        SDL_SetRenderDrawColor(renderer.get(), kClearColor.r, kClearColor.g, kClearColor.b,
            kClearColor.a);
        SDL_RenderClear(renderer.get());

        const Vec2 screenCenter = GetScreenCenter(renderer.get(), *tilemap);
        const int tilesDrawnThisFrame = RenderTilemap(renderer.get(), atlasTexture.get(),
            *tilemap, camera.position.x, camera.position.y, screenCenter.x, screenCenter.y);

        const float boatScreenX =
            (player.x - camera.position.x) + screenCenter.x;
        const float boatScreenY =
            (player.y - camera.position.y) + screenCenter.y;
        const int boatDrawW = kBoatSizePixels;
        const int boatDrawH = kBoatSizePixels;
        SDL_Rect boatDst{
            static_cast<int>(boatScreenX - static_cast<float>(boatDrawW) * 0.5f),
            static_cast<int>(boatScreenY - static_cast<float>(boatDrawH) * 0.5f),
            boatDrawW,
            boatDrawH};
        SDL_RenderCopy(renderer.get(), boatTexture.get(), nullptr, &boatDst);

        if (kDebugOverlay) {
            SDL_Rect debugRect{16, 16, 128, 128};
            SDL_SetRenderDrawColor(renderer.get(), 255, 0, 255, 255);
            SDL_RenderFillRect(renderer.get(), &debugRect);
            SDL_Rect src{0, 0, 32, 32};
            SDL_Rect dst{50, 50, 32, 32};
            SDL_RenderCopy(renderer.get(), atlasTexture.get(), &src, &dst);
        }
        if (kDebugOverlay) {
            int outputW = 0;
            int outputH = 0;
            if (SDL_GetRendererOutputSize(renderer.get(), &outputW, &outputH) == 0) {
                const int maxPreviewW = 256;
                const int maxPreviewH = 256;
                const float scaleW = static_cast<float>(maxPreviewW) /
                    static_cast<float>(atlasWidth > 0 ? atlasWidth : 1);
                const float scaleH = static_cast<float>(maxPreviewH) /
                    static_cast<float>(atlasHeight > 0 ? atlasHeight : 1);
                const float scale = std::min(scaleW, scaleH);
                const int previewW =
                    std::max(1, static_cast<int>(static_cast<float>(atlasWidth) * scale));
                const int previewH =
                    std::max(1, static_cast<int>(static_cast<float>(atlasHeight) * scale));
                const int previewX = std::max(0, outputW - previewW - 10);
                const int previewY = 10;
                SDL_Rect dstAtlasPreview{previewX, previewY, previewW, previewH};
                SDL_RenderCopy(renderer.get(), atlasTexture.get(), nullptr, &dstAtlasPreview);
            }

            const int panelX = 10;
            const int panelY = 200;
            for (int localIndex = 0; localIndex < 48; ++localIndex) {
                const SDL_Rect src = SrcRectForLocalIndex(tilemap->atlas,
                    static_cast<uint32_t>(localIndex));
                const int col = localIndex % 12;
                const int row = localIndex / 12;
                SDL_Rect dst{panelX + col * tilemap->atlas.tileWidth,
                    panelY + row * tilemap->atlas.tileHeight,
                    tilemap->atlas.tileWidth,
                    tilemap->atlas.tileHeight};
                SDL_RenderCopy(renderer.get(), atlasTexture.get(), &src, &dst);
            }
        }
        SDL_RenderPresent(renderer.get());
        ++perfFrameCount;
        perfTilesSum += tilesDrawnThisFrame;
        std::chrono::duration<float> perfElapsed = frameTime - perfWindowStart;
        if (perfElapsed.count() >= 1.0f) {
            const float seconds = perfElapsed.count();
            const float fps = static_cast<float>(perfFrameCount) / seconds;
            const int avgTiles =
                (perfFrameCount > 0) ? (perfTilesSum / perfFrameCount) : 0;
            char title[128];
            std::snprintf(title, sizeof(title), "%s | %.1f FPS | tiles %d", kWindowTitle, fps,
                avgTiles);
            SDL_SetWindowTitle(window.get(), title);
            perfWindowStart = frameTime;
            perfFrameCount = 0;
            perfTilesSum = 0;
        }
        const uint32_t now = SDL_GetTicks();
        if (kDebugOverlay && now - lastTilesLog >= 1000) {
            std::cout << "tilesDrawnThisFrame=" << tilesDrawnThisFrame
                      << " camera=(" << camera.position.x << "," << camera.position.y << ")\n";
            lastTilesLog = now;
        }
    }

    return 0;
}
