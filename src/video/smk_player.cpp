#include "video/smk_player.h"
#include <iostream>
#include <fstream>
#include <cstring>

// Note: Full implementation requires libsmacker or similar
// This is a stub that compiles without external video libraries

namespace mcgng {

SmkPlayer::~SmkPlayer() {
    unload();
}

bool SmkPlayer::load(const std::string& path) {
    unload();

    std::cout << "SmkPlayer: Loading video: " << path << "\n";

    // Read file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "SmkPlayer: Failed to open file: " << path << "\n";
        return false;
    }

    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "SmkPlayer: Failed to read file: " << path << "\n";
        return false;
    }

    return loadFromMemory(data.data(), data.size());
}

bool SmkPlayer::loadFromMemory(const uint8_t* data, size_t size) {
    if (!data || size < 104) {  // Minimum SMK header size
        return false;
    }

    // Check SMK signature
    const char* sig = reinterpret_cast<const char*>(data);
    if (std::strncmp(sig, "SMK2", 4) != 0 && std::strncmp(sig, "SMK4", 4) != 0) {
        std::cerr << "SmkPlayer: Invalid SMK signature\n";
        return false;
    }

    // Parse SMK header (simplified)
    // Full implementation would use libsmacker: smk_open_memory()
    m_width = *reinterpret_cast<const uint32_t*>(data + 4);
    m_height = *reinterpret_cast<const uint32_t*>(data + 8);
    m_frameCount = *reinterpret_cast<const uint32_t*>(data + 12);
    uint32_t frameRateMicros = *reinterpret_cast<const uint32_t*>(data + 16);

    // Convert frame rate
    if (frameRateMicros > 0) {
        m_frameRate = 1000000.0f / static_cast<float>(frameRateMicros);
        m_frameTime = 1.0f / m_frameRate;
    } else {
        m_frameRate = 15.0f;  // Default
        m_frameTime = 1.0f / 15.0f;
    }

    std::cout << "SmkPlayer: Video loaded - " << m_width << "x" << m_height
              << " @ " << m_frameRate << " fps, " << m_frameCount << " frames\n";

    // Allocate frame buffer
    m_frameBuffer.resize(m_width * m_height * 4);  // RGBA
    m_palette.resize(768);  // 256 * 3 (RGB)

    // TODO: Initialize libsmacker decoder
    // m_decoder = smk_open_memory(data, size);

    m_loaded = true;
    m_currentFrame = 0;
    m_timer = 0.0f;
    m_state = VideoState::Stopped;

    return true;
}

void SmkPlayer::unload() {
    if (!m_loaded) {
        return;
    }

    // TODO: Close libsmacker decoder
    // smk_close(m_decoder);
    m_decoder = nullptr;

    if (m_frameTexture != INVALID_TEXTURE) {
        Renderer::instance().destroyTexture(m_frameTexture);
        m_frameTexture = INVALID_TEXTURE;
    }

    m_frameBuffer.clear();
    m_palette.clear();
    m_loaded = false;
    m_state = VideoState::Stopped;
    m_currentFrame = 0;
}

void SmkPlayer::play() {
    if (!m_loaded) {
        return;
    }

    if (m_state == VideoState::Finished) {
        m_currentFrame = 0;
        m_timer = 0.0f;
    }

    m_state = VideoState::Playing;
}

void SmkPlayer::pause() {
    if (m_state == VideoState::Playing) {
        m_state = VideoState::Paused;
    }
}

void SmkPlayer::stop() {
    m_state = VideoState::Stopped;
    m_currentFrame = 0;
    m_timer = 0.0f;
}

void SmkPlayer::update(float deltaTime) {
    if (!m_loaded || m_state != VideoState::Playing) {
        return;
    }

    m_timer += deltaTime;

    // Check if it's time for next frame
    while (m_timer >= m_frameTime && m_state == VideoState::Playing) {
        m_timer -= m_frameTime;
        m_currentFrame++;

        if (m_currentFrame >= m_frameCount) {
            if (m_looping) {
                m_currentFrame = 0;
            } else {
                m_currentFrame = m_frameCount - 1;
                m_state = VideoState::Finished;

                if (m_onFinished) {
                    m_onFinished();
                }
                break;
            }
        }

        // Decode frame
        if (decodeFrame(m_currentFrame)) {
            updateTexture();

            if (m_onFrame) {
                m_onFrame(m_currentFrame);
            }
        }
    }
}

void SmkPlayer::render(int x, int y) {
    if (!m_loaded || m_frameTexture == INVALID_TEXTURE) {
        return;
    }

    Renderer::instance().drawTexture(m_frameTexture, x, y);
}

void SmkPlayer::render(const Rect& destRect) {
    if (!m_loaded || m_frameTexture == INVALID_TEXTURE) {
        return;
    }

    Renderer::instance().drawTexture(m_frameTexture, nullptr, &destRect);
}

float SmkPlayer::getDuration() const {
    if (m_frameRate > 0) {
        return static_cast<float>(m_frameCount) / m_frameRate;
    }
    return 0.0f;
}

float SmkPlayer::getPosition() const {
    if (m_frameRate > 0) {
        return static_cast<float>(m_currentFrame) / m_frameRate;
    }
    return 0.0f;
}

void SmkPlayer::seekFrame(int frame) {
    if (!m_loaded) {
        return;
    }

    m_currentFrame = std::max(0, std::min(frame, m_frameCount - 1));
    m_timer = 0.0f;

    // TODO: Seek in decoder
    // smk_seek_keyframe(m_decoder, m_currentFrame);

    decodeFrame(m_currentFrame);
    updateTexture();
}

void SmkPlayer::seekTime(float time) {
    if (m_frameRate > 0) {
        seekFrame(static_cast<int>(time * m_frameRate));
    }
}

void SmkPlayer::setVolume(float volume) {
    m_volume = std::max(0.0f, std::min(1.0f, volume));
    // TODO: Apply volume to audio decoder
}

bool SmkPlayer::decodeFrame(int frame) {
    if (!m_loaded || frame < 0 || frame >= m_frameCount) {
        return false;
    }

    // TODO: Decode frame with libsmacker
    // smk_enable_video(m_decoder, 1);
    // smk_first(m_decoder);
    // for (int i = 0; i < frame; i++) smk_next(m_decoder);
    // const unsigned char* palette = smk_get_palette(m_decoder);
    // const unsigned char* pixels = smk_get_video(m_decoder);

    // For now, fill with placeholder (gradient based on frame)
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            int idx = (y * m_width + x) * 4;
            m_frameBuffer[idx + 0] = static_cast<uint8_t>((x + frame) % 256);  // R
            m_frameBuffer[idx + 1] = static_cast<uint8_t>((y + frame) % 256);  // G
            m_frameBuffer[idx + 2] = static_cast<uint8_t>(frame % 256);        // B
            m_frameBuffer[idx + 3] = 255;                                       // A
        }
    }

    return true;
}

void SmkPlayer::updateTexture() {
    if (m_frameBuffer.empty()) {
        return;
    }

    auto& renderer = Renderer::instance();

    // Destroy old texture
    if (m_frameTexture != INVALID_TEXTURE) {
        renderer.destroyTexture(m_frameTexture);
    }

    // Create new texture with current frame
    m_frameTexture = renderer.createTexture(m_frameBuffer.data(), m_width, m_height);
}

// VideoManager implementation

VideoManager& VideoManager::instance() {
    static VideoManager instance;
    return instance;
}

bool VideoManager::initialize() {
    if (m_initialized) {
        return true;
    }

    std::cout << "VideoManager: Initializing\n";
    m_initialized = true;
    return true;
}

void VideoManager::shutdown() {
    if (!m_initialized) {
        return;
    }

    std::cout << "VideoManager: Shutting down\n";
    stopVideo();
    m_initialized = false;
}

void VideoManager::update(float deltaTime) {
    if (m_currentVideo && m_currentVideo->isPlaying()) {
        m_currentVideo->update(deltaTime);
    }
}

bool VideoManager::playVideo(const std::string& path, bool fullscreen,
                              VideoFinishedCallback onFinished) {
    if (!m_initialized) {
        return false;
    }

    stopVideo();

    m_currentVideo = std::make_unique<SmkPlayer>();
    if (!m_currentVideo->load(path)) {
        m_currentVideo.reset();
        return false;
    }

    m_fullscreen = fullscreen;
    m_currentVideo->setOnFinished(std::move(onFinished));
    m_currentVideo->play();

    return true;
}

void VideoManager::stopVideo() {
    if (m_currentVideo) {
        m_currentVideo->stop();
        m_currentVideo.reset();
    }
    m_fullscreen = false;
}

bool VideoManager::isVideoPlaying() const {
    return m_currentVideo && m_currentVideo->isPlaying();
}

void VideoManager::skipVideo() {
    if (m_skippable && m_currentVideo) {
        auto callback = m_currentVideo->isFinished() ? nullptr : nullptr;
        stopVideo();
    }
}

void VideoManager::render() {
    if (!m_currentVideo || !m_fullscreen) {
        return;
    }

    auto& renderer = Renderer::instance();

    if (m_fullscreen) {
        // Render centered and scaled to fit screen
        int screenW = renderer.getWidth();
        int screenH = renderer.getHeight();
        int videoW = m_currentVideo->getWidth();
        int videoH = m_currentVideo->getHeight();

        // Calculate scaling to maintain aspect ratio
        float scaleX = static_cast<float>(screenW) / videoW;
        float scaleY = static_cast<float>(screenH) / videoH;
        float scale = std::min(scaleX, scaleY);

        int destW = static_cast<int>(videoW * scale);
        int destH = static_cast<int>(videoH * scale);
        int destX = (screenW - destW) / 2;
        int destY = (screenH - destH) / 2;

        Rect destRect = {destX, destY, destW, destH};
        m_currentVideo->render(destRect);
    }
}

} // namespace mcgng
