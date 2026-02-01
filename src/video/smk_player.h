#ifndef MCGNG_SMK_PLAYER_H
#define MCGNG_SMK_PLAYER_H

#include "graphics/renderer.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace mcgng {

/**
 * Smacker video playback state.
 */
enum class VideoState {
    Stopped,
    Playing,
    Paused,
    Finished
};

/**
 * Video playback callback types.
 */
using VideoFinishedCallback = std::function<void()>;
using VideoFrameCallback = std::function<void(int frame)>;

/**
 * Smacker video player for MCG cutscenes.
 *
 * MCG uses Smacker (.SMK) format for its 45+ intro/outro/mission videos.
 * This player supports:
 * - SMK video decoding
 * - Audio synchronization
 * - Frame-by-frame or real-time playback
 * - Scaling to different resolutions
 *
 * Implementation options:
 * - libsmacker (open source SMK decoder)
 * - FFmpeg (comprehensive but heavier)
 * - Custom decoder based on SMK format spec
 */
class SmkPlayer {
public:
    SmkPlayer() = default;
    ~SmkPlayer();

    SmkPlayer(const SmkPlayer&) = delete;
    SmkPlayer& operator=(const SmkPlayer&) = delete;

    /**
     * Load a Smacker video file.
     * @param path Path to .SMK file
     * @return true on success
     */
    bool load(const std::string& path);

    /**
     * Load a Smacker video from memory.
     * @param data Video data
     * @param size Size of data
     * @return true on success
     */
    bool loadFromMemory(const uint8_t* data, size_t size);

    /**
     * Unload current video.
     */
    void unload();

    /**
     * Check if a video is loaded.
     */
    bool isLoaded() const { return m_loaded; }

    /**
     * Start or resume playback.
     */
    void play();

    /**
     * Pause playback.
     */
    void pause();

    /**
     * Stop playback and reset to beginning.
     */
    void stop();

    /**
     * Update the video player (decode next frame if needed).
     * @param deltaTime Time since last update
     */
    void update(float deltaTime);

    /**
     * Render the current frame.
     * @param x X position
     * @param y Y position
     */
    void render(int x, int y);

    /**
     * Render scaled to fit a rectangle.
     */
    void render(const Rect& destRect);

    /**
     * Get current playback state.
     */
    VideoState getState() const { return m_state; }

    /**
     * Check if video is playing.
     */
    bool isPlaying() const { return m_state == VideoState::Playing; }

    /**
     * Check if video has finished.
     */
    bool isFinished() const { return m_state == VideoState::Finished; }

    // Video properties

    /**
     * Get video width in pixels.
     */
    int getWidth() const { return m_width; }

    /**
     * Get video height in pixels.
     */
    int getHeight() const { return m_height; }

    /**
     * Get frame rate (frames per second).
     */
    float getFrameRate() const { return m_frameRate; }

    /**
     * Get total number of frames.
     */
    int getFrameCount() const { return m_frameCount; }

    /**
     * Get current frame index.
     */
    int getCurrentFrame() const { return m_currentFrame; }

    /**
     * Get total duration in seconds.
     */
    float getDuration() const;

    /**
     * Get current playback position in seconds.
     */
    float getPosition() const;

    // Seeking

    /**
     * Seek to a specific frame.
     */
    void seekFrame(int frame);

    /**
     * Seek to a specific time in seconds.
     */
    void seekTime(float time);

    // Volume control

    /**
     * Set audio volume.
     */
    void setVolume(float volume);
    float getVolume() const { return m_volume; }

    /**
     * Mute/unmute audio.
     */
    void setMuted(bool muted) { m_muted = muted; }
    bool isMuted() const { return m_muted; }

    // Callbacks

    /**
     * Set callback for when video finishes.
     */
    void setOnFinished(VideoFinishedCallback callback) {
        m_onFinished = std::move(callback);
    }

    /**
     * Set callback for each frame.
     */
    void setOnFrame(VideoFrameCallback callback) {
        m_onFrame = std::move(callback);
    }

    // Looping

    /**
     * Enable/disable looping.
     */
    void setLooping(bool loop) { m_looping = loop; }
    bool isLooping() const { return m_looping; }

private:
    bool m_loaded = false;
    VideoState m_state = VideoState::Stopped;

    // Video properties
    int m_width = 0;
    int m_height = 0;
    float m_frameRate = 0.0f;
    int m_frameCount = 0;
    int m_currentFrame = 0;

    // Timing
    float m_frameTime = 0.0f;
    float m_timer = 0.0f;

    // Audio
    float m_volume = 1.0f;
    bool m_muted = false;

    // Playback options
    bool m_looping = false;

    // Rendering
    TextureHandle m_frameTexture = INVALID_TEXTURE;
    std::vector<uint8_t> m_frameBuffer;
    std::vector<uint8_t> m_palette;

    // Callbacks
    VideoFinishedCallback m_onFinished;
    VideoFrameCallback m_onFrame;

    // Internal decoder state (would be smk_t* with libsmacker)
    void* m_decoder = nullptr;

    bool decodeFrame(int frame);
    void updateTexture();
};

/**
 * Video manager for managing multiple videos and cutscene sequences.
 */
class VideoManager {
public:
    static VideoManager& instance();

    /**
     * Initialize the video manager.
     */
    bool initialize();

    /**
     * Shutdown the video manager.
     */
    void shutdown();

    /**
     * Update all active videos.
     */
    void update(float deltaTime);

    /**
     * Play a video file (convenience function).
     * @param path Path to video
     * @param fullscreen Whether to render fullscreen
     * @param onFinished Callback when video finishes
     * @return true if video started successfully
     */
    bool playVideo(const std::string& path, bool fullscreen = true,
                   VideoFinishedCallback onFinished = nullptr);

    /**
     * Stop current video.
     */
    void stopVideo();

    /**
     * Check if a video is currently playing.
     */
    bool isVideoPlaying() const;

    /**
     * Skip current video (if skippable).
     */
    void skipVideo();

    /**
     * Set whether videos can be skipped.
     */
    void setSkippable(bool skippable) { m_skippable = skippable; }

    /**
     * Render current video (if fullscreen video is playing).
     */
    void render();

private:
    VideoManager() = default;

    bool m_initialized = false;
    bool m_skippable = true;
    bool m_fullscreen = false;

    std::unique_ptr<SmkPlayer> m_currentVideo;
};

} // namespace mcgng

#endif // MCGNG_SMK_PLAYER_H
