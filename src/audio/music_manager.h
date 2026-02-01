#ifndef MCGNG_MUSIC_MANAGER_H
#define MCGNG_MUSIC_MANAGER_H

#include <cstdint>
#include <string>
#include <vector>
#include <queue>
#include <memory>

namespace mcgng {

/**
 * Music track handle.
 */
using MusicHandle = uint32_t;
constexpr MusicHandle INVALID_MUSIC = 0;

/**
 * Music playback state.
 */
enum class MusicState {
    Stopped,
    Playing,
    Paused,
    FadingIn,
    FadingOut
};

/**
 * Music manager for background music playback.
 *
 * Features:
 * - WAV music playback (MCG uses WAV for music)
 * - Crossfade between tracks
 * - Playlist management
 * - Volume control with fade
 */
class MusicManager {
public:
    static MusicManager& instance();

    /**
     * Initialize the music manager.
     */
    bool initialize();

    /**
     * Shutdown the music manager.
     */
    void shutdown();

    /**
     * Update the music manager (for fading, etc).
     * @param deltaTime Time since last update
     */
    void update(float deltaTime);

    // Track loading

    /**
     * Load a music track from file.
     * @param path Path to music file
     * @return Music handle or INVALID_MUSIC on failure
     */
    MusicHandle loadTrack(const std::string& path);

    /**
     * Unload a music track.
     */
    void unloadTrack(MusicHandle track);

    /**
     * Unload all tracks.
     */
    void unloadAllTracks();

    // Playback control

    /**
     * Play a music track.
     * @param track Track handle
     * @param fadeInTime Fade in duration in seconds (0 for instant)
     */
    void play(MusicHandle track, float fadeInTime = 0.0f);

    /**
     * Stop current music.
     * @param fadeOutTime Fade out duration in seconds (0 for instant)
     */
    void stop(float fadeOutTime = 0.0f);

    /**
     * Pause current music.
     */
    void pause();

    /**
     * Resume paused music.
     */
    void resume();

    /**
     * Crossfade to a new track.
     * @param track Track to fade to
     * @param fadeTime Duration of crossfade
     */
    void crossfadeTo(MusicHandle track, float fadeTime = 1.0f);

    /**
     * Get current playback state.
     */
    MusicState getState() const { return m_state; }

    /**
     * Check if music is playing.
     */
    bool isPlaying() const { return m_state == MusicState::Playing || m_state == MusicState::FadingIn; }

    // Playlist management

    /**
     * Set playlist from track handles.
     */
    void setPlaylist(const std::vector<MusicHandle>& tracks);

    /**
     * Add track to end of playlist.
     */
    void addToPlaylist(MusicHandle track);

    /**
     * Clear playlist.
     */
    void clearPlaylist();

    /**
     * Play next track in playlist.
     */
    void playNext();

    /**
     * Play previous track in playlist.
     */
    void playPrevious();

    /**
     * Enable/disable shuffle.
     */
    void setShuffle(bool shuffle) { m_shuffle = shuffle; }
    bool isShuffle() const { return m_shuffle; }

    /**
     * Enable/disable loop playlist.
     */
    void setLoopPlaylist(bool loop) { m_loopPlaylist = loop; }
    bool isLoopPlaylist() const { return m_loopPlaylist; }

    // Volume control

    /**
     * Set music volume.
     */
    void setVolume(float volume);
    float getVolume() const { return m_volume; }

    /**
     * Fade volume to target over duration.
     */
    void fadeVolumeTo(float targetVolume, float duration);

private:
    MusicManager() = default;
    ~MusicManager();

    bool m_initialized = false;
    MusicState m_state = MusicState::Stopped;

    // Current track (void* = Mix_Music*)
    void* m_currentMusic = nullptr;
    MusicHandle m_currentTrack = INVALID_MUSIC;

    // Track storage
    std::vector<std::pair<MusicHandle, void*>> m_tracks;
    MusicHandle m_nextTrackHandle = 1;

    // Playlist
    std::vector<MusicHandle> m_playlist;
    int m_playlistIndex = -1;
    bool m_shuffle = false;
    bool m_loopPlaylist = true;

    // Volume
    float m_volume = 1.0f;
    float m_targetVolume = 1.0f;
    float m_fadeSpeed = 0.0f;

    // Fade state
    float m_fadeTimer = 0.0f;
    float m_fadeDuration = 0.0f;
    MusicHandle m_nextTrack = INVALID_MUSIC;

    void* findTrack(MusicHandle handle) const;
    void applyVolume();
};

} // namespace mcgng

#endif // MCGNG_MUSIC_MANAGER_H
