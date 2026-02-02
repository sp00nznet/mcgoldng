#include "graphics/sprite.h"
#include <algorithm>
#include <iostream>
#include <set>

namespace mcgng {

// Sprite implementation

Sprite::~Sprite() {
    destroyTextures();
}

Sprite::Sprite(Sprite&& other) noexcept
    : m_frames(std::move(other.m_frames))
    , m_animations(std::move(other.m_animations))
    , m_currentFrame(other.m_currentFrame)
    , m_currentAnimation(std::move(other.m_currentAnimation))
    , m_animating(other.m_animating)
    , m_animTimer(other.m_animTimer)
    , m_animFrameIndex(other.m_animFrameIndex)
    , m_color(other.m_color)
    , m_flipH(other.m_flipH)
    , m_flipV(other.m_flipV) {
    other.m_currentFrame = 0;
    other.m_animating = false;
}

Sprite& Sprite::operator=(Sprite&& other) noexcept {
    if (this != &other) {
        destroyTextures();
        m_frames = std::move(other.m_frames);
        m_animations = std::move(other.m_animations);
        m_currentFrame = other.m_currentFrame;
        m_currentAnimation = std::move(other.m_currentAnimation);
        m_animating = other.m_animating;
        m_animTimer = other.m_animTimer;
        m_animFrameIndex = other.m_animFrameIndex;
        m_color = other.m_color;
        m_flipH = other.m_flipH;
        m_flipV = other.m_flipV;
        other.m_currentFrame = 0;
        other.m_animating = false;
    }
    return *this;
}

void Sprite::destroyTextures() {
    auto& renderer = Renderer::instance();
    for (auto& frame : m_frames) {
        if (frame.texture != INVALID_TEXTURE) {
            renderer.destroyTexture(frame.texture);
            frame.texture = INVALID_TEXTURE;
        }
    }
    m_frames.clear();
}

bool Sprite::loadFrames(std::vector<SpriteFrame>&& frames) {
    destroyTextures();
    m_frames = std::move(frames);
    m_currentFrame = 0;
    return !m_frames.empty();
}

bool Sprite::createSingle(const uint8_t* pixels, int width, int height) {
    destroyTextures();

    SpriteFrame frame;
    frame.texture = Renderer::instance().createTexture(pixels, width, height);
    frame.width = width;
    frame.height = height;
    frame.offsetX = 0;
    frame.offsetY = 0;

    if (frame.texture == INVALID_TEXTURE) {
        return false;
    }

    m_frames.push_back(frame);
    return true;
}

bool Sprite::createSingleIndexed(const uint8_t* pixels, const uint8_t* palette,
                                  int width, int height) {
    destroyTextures();

    SpriteFrame frame;
    frame.texture = Renderer::instance().createTextureIndexed(pixels, palette, width, height);
    frame.width = width;
    frame.height = height;
    frame.offsetX = 0;
    frame.offsetY = 0;

    if (frame.texture == INVALID_TEXTURE) {
        return false;
    }

    m_frames.push_back(frame);
    return true;
}

bool Sprite::loadFromShape(const ShapeData& shape, const Palette& palette) {
    if (shape.pixels.empty() || shape.width <= 0 || shape.height <= 0) {
        return false;
    }

    destroyTextures();

    // Debug: count non-zero pixels
    size_t nonZeroPixels = 0;
    for (uint8_t p : shape.pixels) {
        if (p != 0) ++nonZeroPixels;
    }
    std::cout << "Sprite::loadFromShape: " << shape.width << "x" << shape.height
              << ", non-zero pixels: " << nonZeroPixels << "/" << shape.pixels.size() << std::endl;

    // Convert indexed pixels to RGBA
    size_t pixelCount = static_cast<size_t>(shape.width * shape.height);
    std::vector<uint8_t> rgba(pixelCount * 4);
    palette.convertToRGBA(shape.pixels.data(), rgba.data(), pixelCount, 0);

    // Debug: count non-transparent RGBA pixels
    size_t visiblePixels = 0;
    for (size_t i = 0; i < pixelCount; ++i) {
        if (rgba[i * 4 + 3] != 0) ++visiblePixels;
    }
    std::cout << "Sprite::loadFromShape: visible (non-transparent) pixels: "
              << visiblePixels << "/" << pixelCount << std::endl;

    // Create texture
    SpriteFrame frame;
    frame.texture = Renderer::instance().createTexture(rgba.data(), shape.width, shape.height);
    frame.width = shape.width;
    frame.height = shape.height;
    frame.offsetX = shape.hotspotX;
    frame.offsetY = shape.hotspotY;

    std::cout << "Sprite::loadFromShape: texture handle = " << frame.texture << std::endl;

    if (frame.texture == INVALID_TEXTURE) {
        std::cerr << "Sprite: Failed to create texture from shape" << std::endl;
        return false;
    }

    m_frames.push_back(frame);
    return true;
}

bool Sprite::loadFromShapes(const ShapeReader& reader, const Palette& palette,
                            uint32_t startIndex, uint32_t count) {
    if (!reader.isLoaded()) {
        return false;
    }

    destroyTextures();

    uint32_t shapeCount = reader.getShapeCount();
    if (startIndex >= shapeCount) {
        return false;
    }

    // Determine how many shapes to load
    uint32_t endIndex = (count == 0) ? shapeCount : std::min(startIndex + count, shapeCount);

    for (uint32_t i = startIndex; i < endIndex; ++i) {
        ShapeData shape = reader.decodeShape(i);
        if (shape.pixels.empty() || shape.width <= 0 || shape.height <= 0) {
            // Skip invalid shapes but continue
            std::cerr << "Sprite: Skipping invalid shape " << i << std::endl;
            continue;
        }

        // Convert indexed pixels to RGBA
        size_t pixelCount = static_cast<size_t>(shape.width * shape.height);
        std::vector<uint8_t> rgba(pixelCount * 4);
        palette.convertToRGBA(shape.pixels.data(), rgba.data(), pixelCount, 0);

        // Create texture
        SpriteFrame frame;
        frame.texture = Renderer::instance().createTexture(rgba.data(), shape.width, shape.height);
        frame.width = shape.width;
        frame.height = shape.height;
        frame.offsetX = shape.hotspotX;
        frame.offsetY = shape.hotspotY;

        if (frame.texture == INVALID_TEXTURE) {
            std::cerr << "Sprite: Failed to create texture for shape " << i << std::endl;
            continue;
        }

        m_frames.push_back(frame);
    }

    std::cout << "Sprite: Loaded " << m_frames.size() << " frames from shapes" << std::endl;
    return !m_frames.empty();
}

void Sprite::addAnimation(const Animation& anim) {
    m_animations[anim.name] = anim;
}

void Sprite::playAnimation(const std::string& name) {
    auto it = m_animations.find(name);
    if (it == m_animations.end()) {
        return;
    }

    if (m_currentAnimation != name) {
        m_currentAnimation = name;
        m_animFrameIndex = 0;
        m_animTimer = 0.0f;
    }

    m_animating = true;

    if (!it->second.frames.empty()) {
        m_currentFrame = it->second.frames[0];
    }
}

void Sprite::stopAnimation() {
    m_animating = false;
}

void Sprite::update(float deltaTime) {
    if (!m_animating || m_currentAnimation.empty()) {
        return;
    }

    auto it = m_animations.find(m_currentAnimation);
    if (it == m_animations.end() || it->second.frames.empty()) {
        return;
    }

    const auto& anim = it->second;
    m_animTimer += deltaTime;

    while (m_animTimer >= anim.frameTime) {
        m_animTimer -= anim.frameTime;
        ++m_animFrameIndex;

        if (m_animFrameIndex >= static_cast<int>(anim.frames.size())) {
            if (anim.loop) {
                m_animFrameIndex = 0;
            } else {
                m_animFrameIndex = static_cast<int>(anim.frames.size()) - 1;
                m_animating = false;
                break;
            }
        }
    }

    m_currentFrame = anim.frames[m_animFrameIndex];
}

void Sprite::draw(int x, int y) {
    if (m_frames.empty() || m_currentFrame < 0 ||
        m_currentFrame >= static_cast<int>(m_frames.size())) {
        return;
    }

    const auto& frame = m_frames[m_currentFrame];
    if (frame.texture == INVALID_TEXTURE) {
        return;
    }

    auto& renderer = Renderer::instance();
    renderer.setDrawColor(m_color);

    int drawX = x - frame.offsetX;
    int drawY = y - frame.offsetY;

    // Debug (only log once per texture)
    static std::set<TextureHandle> loggedTextures;
    if (loggedTextures.find(frame.texture) == loggedTextures.end()) {
        std::cout << "Sprite::draw: tex=" << frame.texture
                  << " at (" << drawX << "," << drawY << ") size "
                  << frame.width << "x" << frame.height << std::endl;
        loggedTextures.insert(frame.texture);
    }

    if (m_flipH || m_flipV) {
        Rect dstRect = {drawX, drawY, frame.width, frame.height};
        renderer.drawTextureEx(frame.texture, nullptr, &dstRect, 0.0f, m_flipH, m_flipV);
    } else {
        renderer.drawTexture(frame.texture, drawX, drawY);
    }
}

void Sprite::drawScaled(int x, int y, float scaleX, float scaleY) {
    if (m_frames.empty() || m_currentFrame < 0 ||
        m_currentFrame >= static_cast<int>(m_frames.size())) {
        return;
    }

    const auto& frame = m_frames[m_currentFrame];
    if (frame.texture == INVALID_TEXTURE) {
        return;
    }

    auto& renderer = Renderer::instance();
    renderer.setDrawColor(m_color);

    int drawX = x - static_cast<int>(frame.offsetX * scaleX);
    int drawY = y - static_cast<int>(frame.offsetY * scaleY);
    int drawW = static_cast<int>(frame.width * scaleX);
    int drawH = static_cast<int>(frame.height * scaleY);

    // Debug (log once per texture for scaled draws)
    static std::set<TextureHandle> loggedScaled;
    if (loggedScaled.find(frame.texture) == loggedScaled.end()) {
        std::cout << "Sprite::drawScaled: tex=" << frame.texture
                  << " at (" << drawX << "," << drawY << ") size "
                  << drawW << "x" << drawH << std::endl;
        loggedScaled.insert(frame.texture);
    }

    Rect dstRect = {drawX, drawY, drawW, drawH};
    renderer.drawTextureEx(frame.texture, nullptr, &dstRect, 0.0f, m_flipH, m_flipV);
}

void Sprite::drawRotated(int x, int y, float angle) {
    if (m_frames.empty() || m_currentFrame < 0 ||
        m_currentFrame >= static_cast<int>(m_frames.size())) {
        return;
    }

    const auto& frame = m_frames[m_currentFrame];
    if (frame.texture == INVALID_TEXTURE) {
        return;
    }

    auto& renderer = Renderer::instance();
    renderer.setDrawColor(m_color);

    int drawX = x - frame.offsetX;
    int drawY = y - frame.offsetY;

    Rect dstRect = {drawX, drawY, frame.width, frame.height};
    renderer.drawTextureEx(frame.texture, nullptr, &dstRect, angle, m_flipH, m_flipV);
}

void Sprite::setFrame(int frame) {
    if (frame >= 0 && frame < static_cast<int>(m_frames.size())) {
        m_currentFrame = frame;
    }
}

int Sprite::getWidth() const {
    if (m_frames.empty() || m_currentFrame < 0 ||
        m_currentFrame >= static_cast<int>(m_frames.size())) {
        return 0;
    }
    return m_frames[m_currentFrame].width;
}

int Sprite::getHeight() const {
    if (m_frames.empty() || m_currentFrame < 0 ||
        m_currentFrame >= static_cast<int>(m_frames.size())) {
        return 0;
    }
    return m_frames[m_currentFrame].height;
}

// SpriteSheet implementation

SpriteSheet::~SpriteSheet() {
    if (m_texture != INVALID_TEXTURE) {
        Renderer::instance().destroyTexture(m_texture);
    }
}

bool SpriteSheet::load(const uint8_t* pixels, int sheetWidth, int sheetHeight,
                        int frameWidth, int frameHeight) {
    if (!pixels || sheetWidth <= 0 || sheetHeight <= 0 ||
        frameWidth <= 0 || frameHeight <= 0) {
        return false;
    }

    if (m_texture != INVALID_TEXTURE) {
        Renderer::instance().destroyTexture(m_texture);
    }

    m_texture = Renderer::instance().createTexture(pixels, sheetWidth, sheetHeight);
    if (m_texture == INVALID_TEXTURE) {
        return false;
    }

    m_sheetWidth = sheetWidth;
    m_sheetHeight = sheetHeight;
    m_frameWidth = frameWidth;
    m_frameHeight = frameHeight;
    m_framesX = sheetWidth / frameWidth;
    m_framesY = sheetHeight / frameHeight;

    return true;
}

bool SpriteSheet::loadIndexed(const uint8_t* pixels, const uint8_t* palette,
                               int sheetWidth, int sheetHeight,
                               int frameWidth, int frameHeight) {
    if (!pixels || !palette || sheetWidth <= 0 || sheetHeight <= 0 ||
        frameWidth <= 0 || frameHeight <= 0) {
        return false;
    }

    if (m_texture != INVALID_TEXTURE) {
        Renderer::instance().destroyTexture(m_texture);
    }

    m_texture = Renderer::instance().createTextureIndexed(pixels, palette, sheetWidth, sheetHeight);
    if (m_texture == INVALID_TEXTURE) {
        return false;
    }

    m_sheetWidth = sheetWidth;
    m_sheetHeight = sheetHeight;
    m_frameWidth = frameWidth;
    m_frameHeight = frameHeight;
    m_framesX = sheetWidth / frameWidth;
    m_framesY = sheetHeight / frameHeight;

    return true;
}

std::unique_ptr<Sprite> SpriteSheet::getSprite(int startFrame, int frameCount) const {
    // Note: This would need to create individual textures for each frame
    // or use source rectangles. For now, return nullptr.
    // Full implementation requires more complex texture handling.
    (void)startFrame;
    (void)frameCount;
    return nullptr;
}

void SpriteSheet::drawFrame(int frame, int x, int y) {
    if (m_texture == INVALID_TEXTURE || frame < 0 || frame >= getFrameCount()) {
        return;
    }

    int frameX = (frame % m_framesX) * m_frameWidth;
    int frameY = (frame / m_framesX) * m_frameHeight;

    Rect srcRect = {frameX, frameY, m_frameWidth, m_frameHeight};
    Rect dstRect = {x, y, m_frameWidth, m_frameHeight};

    Renderer::instance().drawTexture(m_texture, &srcRect, &dstRect);
}

} // namespace mcgng
