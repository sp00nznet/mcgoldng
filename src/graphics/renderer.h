#ifndef MCGNG_RENDERER_H
#define MCGNG_RENDERER_H

#include <cstdint>
#include <string>
#include <memory>

namespace mcgng {

/**
 * Render blend modes.
 */
enum class BlendMode {
    None,
    Alpha,
    Additive,
    Multiply
};

/**
 * Color structure (RGBA).
 */
struct Color {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;

    static Color white() { return {255, 255, 255, 255}; }
    static Color black() { return {0, 0, 0, 255}; }
    static Color red() { return {255, 0, 0, 255}; }
    static Color green() { return {0, 255, 0, 255}; }
    static Color blue() { return {0, 0, 255, 255}; }
    static Color transparent() { return {0, 0, 0, 0}; }
};

/**
 * Rectangle structure.
 */
struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool contains(int px, int py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }

    bool intersects(const Rect& other) const {
        return !(x >= other.x + other.width || x + width <= other.x ||
                 y >= other.y + other.height || y + height <= other.y);
    }
};

/**
 * Texture handle.
 */
using TextureHandle = uint32_t;
constexpr TextureHandle INVALID_TEXTURE = 0;

/**
 * Main renderer class.
 * Handles window management and 2D rendering via SDL2 + OpenGL.
 */
class Renderer {
public:
    static Renderer& instance();

    /**
     * Initialize the renderer.
     * @param windowTitle Window title
     * @param width Window width
     * @param height Window height
     * @param fullscreen Start in fullscreen mode
     * @return true on success
     */
    bool initialize(const std::string& windowTitle, int width, int height, bool fullscreen = false);

    /**
     * Shutdown the renderer.
     */
    void shutdown();

    /**
     * Check if renderer is initialized.
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * Begin a new frame.
     */
    void beginFrame();

    /**
     * End the current frame and present to screen.
     */
    void endFrame();

    /**
     * Clear the screen with a color.
     */
    void clear(const Color& color = Color::black());

    /**
     * Set the current blend mode.
     */
    void setBlendMode(BlendMode mode);

    /**
     * Set the current draw color.
     */
    void setDrawColor(const Color& color);

    /**
     * Draw a filled rectangle.
     */
    void drawRect(const Rect& rect);

    /**
     * Draw a rectangle outline.
     */
    void drawRectOutline(const Rect& rect);

    /**
     * Draw a line.
     */
    void drawLine(int x1, int y1, int x2, int y2);

    /**
     * Draw a point.
     */
    void drawPoint(int x, int y);

    /**
     * Create a texture from raw pixel data.
     * @param pixels RGBA pixel data
     * @param width Texture width
     * @param height Texture height
     * @return Texture handle or INVALID_TEXTURE on failure
     */
    TextureHandle createTexture(const uint8_t* pixels, int width, int height);

    /**
     * Create a texture from indexed (paletted) pixel data.
     * @param pixels 8-bit indexed pixel data
     * @param palette 256-color palette (768 bytes RGB)
     * @param width Texture width
     * @param height Texture height
     * @return Texture handle or INVALID_TEXTURE on failure
     */
    TextureHandle createTextureIndexed(const uint8_t* pixels, const uint8_t* palette,
                                       int width, int height);

    /**
     * Destroy a texture.
     */
    void destroyTexture(TextureHandle texture);

    /**
     * Draw a texture.
     */
    void drawTexture(TextureHandle texture, int x, int y);

    /**
     * Draw a texture with source and destination rectangles.
     */
    void drawTexture(TextureHandle texture, const Rect* srcRect, const Rect* dstRect);

    /**
     * Draw a texture with rotation and flip.
     */
    void drawTextureEx(TextureHandle texture, const Rect* srcRect, const Rect* dstRect,
                       float angle, bool flipH = false, bool flipV = false);

    /**
     * Get window dimensions.
     */
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

    /**
     * Set viewport/render scale for resolution independence.
     * @param logicalWidth Logical (game) width
     * @param logicalHeight Logical (game) height
     */
    void setLogicalSize(int logicalWidth, int logicalHeight);

    /**
     * Toggle fullscreen mode.
     */
    void toggleFullscreen();

    /**
     * Set VSync.
     */
    void setVSync(bool enabled);

    /**
     * Get the native window handle (platform-specific).
     */
    void* getNativeHandle() const { return m_window; }

private:
    Renderer() = default;
    ~Renderer();

    bool m_initialized = false;
    void* m_window = nullptr;      // SDL_Window*
    void* m_glContext = nullptr;   // SDL_GLContext
    void* m_renderer = nullptr;    // SDL_Renderer* (for 2D fallback)

    int m_width = 0;
    int m_height = 0;
    int m_logicalWidth = 0;
    int m_logicalHeight = 0;
    bool m_fullscreen = false;

    Color m_drawColor = Color::white();
    BlendMode m_blendMode = BlendMode::Alpha;
};

} // namespace mcgng

#endif // MCGNG_RENDERER_H
