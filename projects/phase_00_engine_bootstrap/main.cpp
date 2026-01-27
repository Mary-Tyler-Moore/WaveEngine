#include <SDL.h>
#include <SDL_image.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr Uint8 kOceanRed = 150;
constexpr Uint8 kOceanGreen = 200;
constexpr Uint8 kOceanBlue = 220;
constexpr Uint8 kOceanAlpha = 255;

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

struct ImgGuard {
    explicit ImgGuard(int flags) : ok_((IMG_Init(flags) & flags) == flags) {}
    ~ImgGuard() {
        if (ok_) {
            IMG_Quit();
        }
    }

    ImgGuard(const ImgGuard&) = delete;
    ImgGuard& operator=(const ImgGuard&) = delete;

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

TexturePtr LoadTexture(SDL_Renderer* renderer, const std::filesystem::path& path) {
    SDL_Texture* texture = IMG_LoadTexture(renderer, path.string().c_str());
    if (!texture) {
        std::cerr << "Failed to load texture '" << path.string() << "': " << IMG_GetError()
                  << "\n";
        return TexturePtr(nullptr, SDL_DestroyTexture);
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

    SDL_Surface* rawSurface =
        SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!rawSurface) {
        std::cerr << "SDL_CreateRGBSurfaceWithFormat failed: " << SDL_GetError() << "\n";
        return false;
    }
    std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface(rawSurface, SDL_FreeSurface);

    if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, surface->pixels,
            surface->pitch)
        != 0) {
        std::cerr << "SDL_RenderReadPixels failed: " << SDL_GetError() << "\n";
        return false;
    }

    if (IMG_SavePNG(surface.get(), outputPath.string().c_str()) != 0) {
        std::cerr << "IMG_SavePNG failed: " << IMG_GetError() << "\n";
        return false;
    }

    std::cout << "Saved screenshot: " << outputPath.string() << "\n";
    return true;
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

    ImgGuard img(IMG_INIT_PNG);
    if (!img.ok()) {
        std::cerr << "IMG_Init failed: " << IMG_GetError() << "\n";
        return 1;
    }

    WindowPtr window(SDL_CreateWindow(
                         "Phase 00 - Engine Bootstrap",
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

    const auto assetPath = FindAssetPath("assets/boat-64.png");
    if (!assetPath) {
        std::cerr << "Could not locate assets/boat-64.png\n";
        return 1;
    }

    TexturePtr boatTexture = LoadTexture(renderer.get(), *assetPath);
    if (!boatTexture) {
        return 1;
    }

    int textureWidth = 0;
    int textureHeight = 0;
    if (SDL_QueryTexture(boatTexture.get(), nullptr, nullptr, &textureWidth, &textureHeight) != 0) {
        std::cerr << "SDL_QueryTexture failed: " << SDL_GetError() << "\n";
        return 1;
    }

    bool running = true;
    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event) == 1) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_F12) {
                    SaveScreenshotPNG(renderer.get(),
                        "assets/screenshots/phase_00_engine_bootstrap.png");
                }
            }
        }

        int outputWidth = 0;
        int outputHeight = 0;
        if (SDL_GetRendererOutputSize(renderer.get(), &outputWidth, &outputHeight) != 0) {
            std::cerr << "SDL_GetRendererOutputSize failed: " << SDL_GetError() << "\n";
            return 1;
        }

        SDL_SetRenderDrawColor(renderer.get(), kOceanRed, kOceanGreen, kOceanBlue, kOceanAlpha);
        SDL_RenderClear(renderer.get());

        SDL_Rect destination{
            (outputWidth - textureWidth) / 2,
            (outputHeight - textureHeight) / 2,
            textureWidth,
            textureHeight,
        };

        SDL_RenderCopy(renderer.get(), boatTexture.get(), nullptr, &destination);
        SDL_RenderPresent(renderer.get());
    }

    return 0;
}
