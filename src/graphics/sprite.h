#ifndef MCGNG_SPRITE_H
#define MCGNG_SPRITE_H

#include "graphics/renderer.h"
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace mcgng {

/**
 * Single frame of a sprite.
 */
struct SpriteFrame {
    TextureHandle texture = INVALID_TEXTURE;
    int width = 0;
    int height = 0;
    int offsetX = 0;    // Hotspot offset
    int offsetY = 0;
};

/**
 * Animation sequence.
 */
struct Animation {
    std::string name;
    std::vector<int> frames;    // Frame indices
    float frameTime = 0.1f;     // Seconds per frame
    bool loop = true;
};

/**
 * Sprite class - handles multi-frame sprites with animation.
 */
class Sprite {
public:
    Sprite() = default;
    ~Sprite();

    Sprite(const Sprite&) = delete;
    Sprite& operator=(const Sprite&) = delete;
    Sprite(Sprite&&) noexcept;
    Sprite& operator=(Sprite&&) noexcept;

    /**
     * Load sprite from raw frame data.
     * @param frames Vector of frame data
     * @return true on success
     */
    bool loadFrames(std::vector<SpriteFrame>&& frames);

    /**
     * Create a single-frame sprite from pixel data.
     * @param pixels RGBA pixel data
     * @param width Frame width
     * @param height Frame height
     * @return true on success
     */
    bool createSingle(const uint8_t* pixels, int width, int height);

    /**
     * Create a single-frame sprite from indexed pixel data.
     * @param pixels 8-bit indexed pixel data
     * @param palette 256-color palette
     * @param width Frame width
     * @param height Frame height
     * @return true on success
     */
    bool createSingleIndexed(const uint8_t* pixels, const uint8_t* palette,
                             int width, int height);

    /**
     * Add an animation sequence.
     */
    void addAnimation(const Animation& anim);

    /**
     * Play an animation by name.
     */
    void playAnimation(const std::string& name);

    /**
     * Stop animation.
     */
    void stopAnimation();

    /**
     * Update animation state.
     * @param deltaTime Time since last update
     */
    void update(float deltaTime);

    /**
     * Draw the sprite at a position.
     * @param x X position
     * @param y Y position
     */
    void draw(int x, int y);

    /**
     * Draw with scaling.
     */
    void drawScaled(int x, int y, float scaleX, float scaleY);

    /**
     * Draw with rotation.
     */
    void drawRotated(int x, int y, float angle);

    /**
     * Get current frame index.
     */
    int getCurrentFrame() const { return m_currentFrame; }

    /**
     * Set current frame directly.
     */
    void setFrame(int frame);

    /**
     * Get number of frames.
     */
    int getFrameCount() const { return static_cast<int>(m_frames.size()); }

    /**
     * Get frame dimensions.
     */
    int getWidth() const;
    int getHeight() const;

    /**
     * Check if loaded.
     */
    bool isLoaded() const { return !m_frames.empty(); }

    /**
     * Check if animating.
     */
    bool isAnimating() const { return m_animating; }

    /**
     * Set color modulation.
     */
    void setColor(const Color& color) { m_color = color; }

    /**
     * Set alpha.
     */
    void setAlpha(uint8_t alpha) { m_color.a = alpha; }

    /**
     * Set flip state.
     */
    void setFlip(bool horizontal, bool vertical) {
        m_flipH = horizontal;
        m_flipV = vertical;
    }

private:
    std::vector<SpriteFrame> m_frames;
    std::unordered_map<std::string, Animation> m_animations;

    int m_currentFrame = 0;
    std::string m_currentAnimation;
    bool m_animating = false;
    float m_animTimer = 0.0f;
    int m_animFrameIndex = 0;

    Color m_color = Color::white();
    bool m_flipH = false;
    bool m_flipV = false;

    void destroyTextures();
};

/**
 * Sprite sheet - grid-based sprite organization.
 */
class SpriteSheet {
public:
    SpriteSheet() = default;
    ~SpriteSheet();

    /**
     * Load from RGBA pixel data.
     * @param pixels RGBA pixel data
     * @param sheetWidth Total sheet width
     * @param sheetHeight Total sheet height
     * @param frameWidth Width of each frame
     * @param frameHeight Height of each frame
     * @return true on success
     */
    bool load(const uint8_t* pixels, int sheetWidth, int sheetHeight,
              int frameWidth, int frameHeight);

    /**
     * Load from indexed pixel data.
     */
    bool loadIndexed(const uint8_t* pixels, const uint8_t* palette,
                     int sheetWidth, int sheetHeight,
                     int frameWidth, int frameHeight);

    /**
     * Get a sprite with specific frames.
     * @param startFrame First frame index
     * @param frameCount Number of frames (0 = single frame)
     * @return Unique pointer to sprite
     */
    std::unique_ptr<Sprite> getSprite(int startFrame, int frameCount = 1) const;

    /**
     * Draw a specific frame directly.
     */
    void drawFrame(int frame, int x, int y);

    /**
     * Get frame count.
     */
    int getFrameCount() const { return m_framesX * m_framesY; }

    /**
     * Get frame dimensions.
     */
    int getFrameWidth() const { return m_frameWidth; }
    int getFrameHeight() const { return m_frameHeight; }

private:
    TextureHandle m_texture = INVALID_TEXTURE;
    int m_sheetWidth = 0;
    int m_sheetHeight = 0;
    int m_frameWidth = 0;
    int m_frameHeight = 0;
    int m_framesX = 0;
    int m_framesY = 0;
};

} // namespace mcgng

#endif // MCGNG_SPRITE_H
