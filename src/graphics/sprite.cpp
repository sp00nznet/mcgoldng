#include "graphics/sprite.h"
#include <algorithm>

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
