#include "graphics/renderer.h"
#include <iostream>
#include <vector>

#ifdef MCGNG_HAS_SDL2
#include <SDL.h>
#include <unordered_map>

namespace mcgng {

// Internal texture storage
struct TextureData {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
};

static std::unordered_map<TextureHandle, TextureData> s_textures;
static TextureHandle s_nextTextureId = 1;

Renderer& Renderer::instance() {
    static Renderer instance;
    return instance;
}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(const std::string& windowTitle, int width, int height, bool fullscreen) {
    if (m_initialized) {
        return true;
    }

    // Initialize SDL video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Renderer: Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create window
    Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    SDL_Window* window = SDL_CreateWindow(
        windowTitle.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        windowFlags
    );

    if (!window) {
        std::cerr << "Renderer: Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    m_window = window;
    m_width = width;
    m_height = height;
    m_fullscreen = fullscreen;

    // Create SDL renderer (hardware accelerated)
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer) {
        std::cerr << "Renderer: Failed to create SDL renderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    m_renderer = renderer;

    // Set default blend mode
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Set logical size for resolution independence
    m_logicalWidth = width;
    m_logicalHeight = height;

    std::cout << "Renderer: Initialized " << width << "x" << height;
    if (fullscreen) std::cout << " (fullscreen)";
    std::cout << std::endl;

    m_initialized = true;
    return true;
}

void Renderer::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Destroy all textures
    for (auto& pair : s_textures) {
        if (pair.second.texture) {
            SDL_DestroyTexture(pair.second.texture);
        }
    }
    s_textures.clear();
    s_nextTextureId = 1;

    // Destroy renderer and window
    if (m_renderer) {
        SDL_DestroyRenderer(static_cast<SDL_Renderer*>(m_renderer));
        m_renderer = nullptr;
    }

    if (m_window) {
        SDL_DestroyWindow(static_cast<SDL_Window*>(m_window));
        m_window = nullptr;
    }

    SDL_Quit();
    m_initialized = false;

    std::cout << "Renderer: Shutdown complete" << std::endl;
}

void Renderer::beginFrame() {
    // Nothing special needed for SDL2 renderer
}

void Renderer::endFrame() {
    if (m_renderer) {
        SDL_RenderPresent(static_cast<SDL_Renderer*>(m_renderer));
    }
}

void Renderer::clear(const Color& color) {
    if (!m_renderer) return;

    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(m_renderer);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer);
}

void Renderer::setBlendMode(BlendMode mode) {
    if (!m_renderer) return;

    m_blendMode = mode;
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(m_renderer);

    switch (mode) {
        case BlendMode::None:
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            break;
        case BlendMode::Alpha:
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            break;
        case BlendMode::Additive:
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
            break;
        case BlendMode::Multiply:
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_MOD);
            break;
    }
}

void Renderer::setDrawColor(const Color& color) {
    m_drawColor = color;
    if (m_renderer) {
        SDL_SetRenderDrawColor(static_cast<SDL_Renderer*>(m_renderer),
                               color.r, color.g, color.b, color.a);
    }
}

void Renderer::drawRect(const Rect& rect) {
    if (!m_renderer) return;

    SDL_Rect sdlRect = {rect.x, rect.y, rect.width, rect.height};
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(m_renderer);
    SDL_SetRenderDrawColor(renderer, m_drawColor.r, m_drawColor.g, m_drawColor.b, m_drawColor.a);
    SDL_RenderFillRect(renderer, &sdlRect);
}

void Renderer::drawRectOutline(const Rect& rect) {
    if (!m_renderer) return;

    SDL_Rect sdlRect = {rect.x, rect.y, rect.width, rect.height};
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(m_renderer);
    SDL_SetRenderDrawColor(renderer, m_drawColor.r, m_drawColor.g, m_drawColor.b, m_drawColor.a);
    SDL_RenderDrawRect(renderer, &sdlRect);
}

void Renderer::drawLine(int x1, int y1, int x2, int y2) {
    if (!m_renderer) return;

    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(m_renderer);
    SDL_SetRenderDrawColor(renderer, m_drawColor.r, m_drawColor.g, m_drawColor.b, m_drawColor.a);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void Renderer::drawPoint(int x, int y) {
    if (!m_renderer) return;

    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(m_renderer);
    SDL_SetRenderDrawColor(renderer, m_drawColor.r, m_drawColor.g, m_drawColor.b, m_drawColor.a);
    SDL_RenderDrawPoint(renderer, x, y);
}

TextureHandle Renderer::createTexture(const uint8_t* pixels, int width, int height) {
    if (!m_renderer || !pixels || width <= 0 || height <= 0) {
        return INVALID_TEXTURE;
    }

    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(m_renderer);

    // Create texture
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        width, height
    );

    if (!texture) {
        std::cerr << "Renderer: Failed to create texture: " << SDL_GetError() << std::endl;
        return INVALID_TEXTURE;
    }

    // Upload pixel data
    if (SDL_UpdateTexture(texture, nullptr, pixels, width * 4) < 0) {
        std::cerr << "Renderer: Failed to update texture: " << SDL_GetError() << std::endl;
        SDL_DestroyTexture(texture);
        return INVALID_TEXTURE;
    }

    // Enable alpha blending on texture
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    // Store texture
    TextureHandle handle = s_nextTextureId++;
    s_textures[handle] = {texture, width, height};

    return handle;
}

TextureHandle Renderer::createTextureIndexed(const uint8_t* pixels, const uint8_t* palette,
                                              int width, int height) {
    if (!pixels || !palette || width <= 0 || height <= 0) {
        return INVALID_TEXTURE;
    }

    // Convert indexed to RGBA
    std::vector<uint8_t> rgba(width * height * 4);

    for (int i = 0; i < width * height; ++i) {
        uint8_t index = pixels[i];
        rgba[i * 4 + 0] = palette[index * 3 + 0];  // R
        rgba[i * 4 + 1] = palette[index * 3 + 1];  // G
        rgba[i * 4 + 2] = palette[index * 3 + 2];  // B
        rgba[i * 4 + 3] = (index == 0) ? 0 : 255;  // A (index 0 = transparent)
    }

    return createTexture(rgba.data(), width, height);
}

void Renderer::destroyTexture(TextureHandle texture) {
    auto it = s_textures.find(texture);
    if (it != s_textures.end()) {
        if (it->second.texture) {
            SDL_DestroyTexture(it->second.texture);
        }
        s_textures.erase(it);
    }
}

void Renderer::drawTexture(TextureHandle texture, int x, int y) {
    auto it = s_textures.find(texture);
    if (it == s_textures.end() || !it->second.texture || !m_renderer) {
        return;
    }

    SDL_Rect dstRect = {x, y, it->second.width, it->second.height};
    SDL_RenderCopy(static_cast<SDL_Renderer*>(m_renderer), it->second.texture, nullptr, &dstRect);
}

void Renderer::drawTexture(TextureHandle texture, const Rect* srcRect, const Rect* dstRect) {
    auto it = s_textures.find(texture);
    if (it == s_textures.end() || !it->second.texture || !m_renderer) {
        return;
    }

    SDL_Rect* src = nullptr;
    SDL_Rect* dst = nullptr;
    SDL_Rect srcSDL, dstSDL;

    if (srcRect) {
        srcSDL = {srcRect->x, srcRect->y, srcRect->width, srcRect->height};
        src = &srcSDL;
    }

    if (dstRect) {
        dstSDL = {dstRect->x, dstRect->y, dstRect->width, dstRect->height};
        dst = &dstSDL;
    }

    SDL_RenderCopy(static_cast<SDL_Renderer*>(m_renderer), it->second.texture, src, dst);
}

void Renderer::drawTextureEx(TextureHandle texture, const Rect* srcRect, const Rect* dstRect,
                              float angle, bool flipH, bool flipV) {
    auto it = s_textures.find(texture);
    if (it == s_textures.end() || !it->second.texture || !m_renderer) {
        return;
    }

    SDL_Rect* src = nullptr;
    SDL_Rect* dst = nullptr;
    SDL_Rect srcSDL, dstSDL;

    if (srcRect) {
        srcSDL = {srcRect->x, srcRect->y, srcRect->width, srcRect->height};
        src = &srcSDL;
    }

    if (dstRect) {
        dstSDL = {dstRect->x, dstRect->y, dstRect->width, dstRect->height};
        dst = &dstSDL;
    }

    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if (flipH) flip = static_cast<SDL_RendererFlip>(flip | SDL_FLIP_HORIZONTAL);
    if (flipV) flip = static_cast<SDL_RendererFlip>(flip | SDL_FLIP_VERTICAL);

    SDL_RenderCopyEx(static_cast<SDL_Renderer*>(m_renderer), it->second.texture,
                     src, dst, angle, nullptr, flip);
}

void Renderer::setLogicalSize(int logicalWidth, int logicalHeight) {
    m_logicalWidth = logicalWidth;
    m_logicalHeight = logicalHeight;

    if (m_renderer) {
        SDL_RenderSetLogicalSize(static_cast<SDL_Renderer*>(m_renderer),
                                 logicalWidth, logicalHeight);
    }
}

void Renderer::toggleFullscreen() {
    if (!m_window) return;

    SDL_Window* window = static_cast<SDL_Window*>(m_window);
    m_fullscreen = !m_fullscreen;

    if (m_fullscreen) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowFullscreen(window, 0);
    }
}

void Renderer::setVSync(bool enabled) {
    (void)enabled;
    // VSync is set at renderer creation time in SDL2
}

} // namespace mcgng

#else // !MCGNG_HAS_SDL2

// Stub implementation when SDL2 is not available
namespace mcgng {

Renderer& Renderer::instance() {
    static Renderer instance;
    return instance;
}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(const std::string& windowTitle, int width, int height, bool fullscreen) {
    if (m_initialized) {
        return true;
    }

    m_width = width;
    m_height = height;
    m_logicalWidth = width;
    m_logicalHeight = height;
    m_fullscreen = fullscreen;

    std::cout << "Renderer: Initializing " << width << "x" << height << " (stub mode - no SDL2)" << std::endl;
    std::cout << "Renderer: Window title: " << windowTitle << std::endl;

    m_initialized = true;
    return true;
}

void Renderer::shutdown() {
    if (!m_initialized) return;
    std::cout << "Renderer: Shutdown (stub)" << std::endl;
    m_initialized = false;
}

void Renderer::beginFrame() {}
void Renderer::endFrame() {}
void Renderer::clear(const Color&) {}
void Renderer::setBlendMode(BlendMode mode) { m_blendMode = mode; }
void Renderer::setDrawColor(const Color& color) { m_drawColor = color; }
void Renderer::drawRect(const Rect&) {}
void Renderer::drawRectOutline(const Rect&) {}
void Renderer::drawLine(int, int, int, int) {}
void Renderer::drawPoint(int, int) {}

TextureHandle Renderer::createTexture(const uint8_t*, int, int) {
    static TextureHandle nextHandle = 1;
    return nextHandle++;
}

TextureHandle Renderer::createTextureIndexed(const uint8_t* pixels, const uint8_t* palette,
                                              int width, int height) {
    if (!pixels || !palette || width <= 0 || height <= 0) {
        return INVALID_TEXTURE;
    }
    return createTexture(nullptr, width, height);
}

void Renderer::destroyTexture(TextureHandle) {}
void Renderer::drawTexture(TextureHandle, int, int) {}
void Renderer::drawTexture(TextureHandle, const Rect*, const Rect*) {}
void Renderer::drawTextureEx(TextureHandle, const Rect*, const Rect*, float, bool, bool) {}
void Renderer::setLogicalSize(int w, int h) { m_logicalWidth = w; m_logicalHeight = h; }
void Renderer::toggleFullscreen() { m_fullscreen = !m_fullscreen; }
void Renderer::setVSync(bool) {}

} // namespace mcgng

#endif // MCGNG_HAS_SDL2
