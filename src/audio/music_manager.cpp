#include "audio/music_manager.h"
#include <iostream>
#include <algorithm>
#include <random>

#ifdef MCGNG_HAS_SDL2_MIXER
#include <SDL_mixer.h>
#endif

namespace mcgng {

MusicManager& MusicManager::instance() {
    static MusicManager instance;
    return instance;
}

MusicManager::~MusicManager() {
    if (m_initialized) {
        shutdown();
    }
}

bool MusicManager::initialize() {
    if (m_initialized) {
        return true;
    }

    std::cout << "MusicManager: Initializing\n";

    // Audio system should already be initialized
    m_initialized = true;
    m_state = MusicState::Stopped;

#ifdef MCGNG_HAS_SDL2_MIXER
    std::cout << "MusicManager: SDL2_mixer available for music playback\n";
#else
    std::cout << "MusicManager: Running in stub mode (no audio)\n";
#endif
    return true;
}

void MusicManager::shutdown() {
    if (!m_initialized) {
        return;
    }

    std::cout << "MusicManager: Shutting down\n";

    stop(0.0f);
    unloadAllTracks();

    m_initialized = false;
}

void MusicManager::update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Handle volume fading
    if (m_fadeSpeed != 0.0f) {
        if (m_fadeSpeed > 0.0f) {
            m_volume = std::min(m_volume + m_fadeSpeed * deltaTime, m_targetVolume);
            if (m_volume >= m_targetVolume) {
                m_volume = m_targetVolume;
                m_fadeSpeed = 0.0f;
            }
        } else {
            m_volume = std::max(m_volume + m_fadeSpeed * deltaTime, m_targetVolume);
            if (m_volume <= m_targetVolume) {
                m_volume = m_targetVolume;
                m_fadeSpeed = 0.0f;
            }
        }
        applyVolume();
    }

    // Handle crossfade
    if (m_state == MusicState::FadingOut && m_fadeDuration > 0.0f) {
        m_fadeTimer += deltaTime;
        float progress = m_fadeTimer / m_fadeDuration;

        if (progress >= 1.0f) {
            // Fade complete
            if (m_nextTrack != INVALID_MUSIC) {
                // Start next track
                m_currentTrack = m_nextTrack;
                m_nextTrack = INVALID_MUSIC;
                m_state = MusicState::FadingIn;
                m_fadeTimer = 0.0f;
                m_volume = 0.0f;
                applyVolume();

                // TODO: Actually start playing the track
                // Mix_PlayMusic(findTrack(m_currentTrack), -1);
            } else {
                m_state = MusicState::Stopped;
            }
        } else {
            // Continue fading
            m_volume = 1.0f - progress;
            applyVolume();
        }
    }

    if (m_state == MusicState::FadingIn && m_fadeDuration > 0.0f) {
        m_fadeTimer += deltaTime;
        float progress = m_fadeTimer / m_fadeDuration;

        if (progress >= 1.0f) {
            m_volume = 1.0f;
            m_state = MusicState::Playing;
        } else {
            m_volume = progress;
        }
        applyVolume();
    }

    // Check for track end and auto-advance playlist
    // TODO: Check if music finished with SDL2_mixer callback
    // if (!Mix_PlayingMusic() && m_state == MusicState::Playing) {
    //     playNext();
    // }
}

MusicHandle MusicManager::loadTrack(const std::string& path) {
    if (!m_initialized) {
        return INVALID_MUSIC;
    }

    std::cout << "MusicManager: Loading track: " << path << "\n";

#ifdef MCGNG_HAS_SDL2_MIXER
    Mix_Music* music = Mix_LoadMUS(path.c_str());
    if (!music) {
        std::cerr << "MusicManager: Failed to load track: " << path << " - " << Mix_GetError() << "\n";
        return INVALID_MUSIC;
    }

    MusicHandle handle = m_nextTrackHandle++;
    m_tracks.push_back({handle, music});
    return handle;
#else
    MusicHandle handle = m_nextTrackHandle++;
    m_tracks.push_back({handle, nullptr});
    return handle;
#endif
}

void MusicManager::unloadTrack(MusicHandle track) {
    auto it = std::find_if(m_tracks.begin(), m_tracks.end(),
        [track](const auto& pair) { return pair.first == track; });

    if (it != m_tracks.end()) {
#ifdef MCGNG_HAS_SDL2_MIXER
        if (it->second) {
            Mix_FreeMusic(static_cast<Mix_Music*>(it->second));
        }
#endif
        m_tracks.erase(it);
    }
}

void MusicManager::unloadAllTracks() {
#ifdef MCGNG_HAS_SDL2_MIXER
    for (auto& pair : m_tracks) {
        if (pair.second) {
            Mix_FreeMusic(static_cast<Mix_Music*>(pair.second));
        }
    }
#endif
    m_tracks.clear();
}

void MusicManager::play(MusicHandle track, float fadeInTime) {
    if (!m_initialized || track == INVALID_MUSIC) {
        return;
    }

    void* music = findTrack(track);
    if (!music) {
        std::cerr << "MusicManager: Track not found\n";
        return;
    }

    m_currentTrack = track;
    m_currentMusic = music;

    if (fadeInTime > 0.0f) {
        m_volume = 0.0f;
        m_state = MusicState::FadingIn;
        m_fadeDuration = fadeInTime;
        m_fadeTimer = 0.0f;
    } else {
        m_volume = 1.0f;
        m_state = MusicState::Playing;
    }

    applyVolume();

#ifdef MCGNG_HAS_SDL2_MIXER
    if (fadeInTime > 0.0f) {
        Mix_FadeInMusic(static_cast<Mix_Music*>(music), -1, static_cast<int>(fadeInTime * 1000));
    } else {
        Mix_PlayMusic(static_cast<Mix_Music*>(music), -1);
    }
#endif
}

void MusicManager::stop(float fadeOutTime) {
    if (!m_initialized || m_state == MusicState::Stopped) {
        return;
    }

    if (fadeOutTime > 0.0f) {
#ifdef MCGNG_HAS_SDL2_MIXER
        Mix_FadeOutMusic(static_cast<int>(fadeOutTime * 1000));
#endif
        m_state = MusicState::FadingOut;
        m_fadeDuration = fadeOutTime;
        m_fadeTimer = 0.0f;
        m_nextTrack = INVALID_MUSIC;
    } else {
#ifdef MCGNG_HAS_SDL2_MIXER
        Mix_HaltMusic();
#endif
        m_state = MusicState::Stopped;
        m_currentTrack = INVALID_MUSIC;
        m_currentMusic = nullptr;
    }
}

void MusicManager::pause() {
    if (m_state == MusicState::Playing) {
#ifdef MCGNG_HAS_SDL2_MIXER
        Mix_PauseMusic();
#endif
        m_state = MusicState::Paused;
    }
}

void MusicManager::resume() {
    if (m_state == MusicState::Paused) {
#ifdef MCGNG_HAS_SDL2_MIXER
        Mix_ResumeMusic();
#endif
        m_state = MusicState::Playing;
    }
}

void MusicManager::crossfadeTo(MusicHandle track, float fadeTime) {
    if (!m_initialized || track == INVALID_MUSIC) {
        return;
    }

    if (m_state == MusicState::Stopped) {
        play(track, fadeTime);
        return;
    }

    m_nextTrack = track;
    m_state = MusicState::FadingOut;
    m_fadeDuration = fadeTime / 2.0f;  // Split fade time between out and in
    m_fadeTimer = 0.0f;
}

void MusicManager::setPlaylist(const std::vector<MusicHandle>& tracks) {
    m_playlist = tracks;
    m_playlistIndex = -1;
}

void MusicManager::addToPlaylist(MusicHandle track) {
    m_playlist.push_back(track);
}

void MusicManager::clearPlaylist() {
    m_playlist.clear();
    m_playlistIndex = -1;
}

void MusicManager::playNext() {
    if (m_playlist.empty()) {
        return;
    }

    if (m_shuffle) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, static_cast<int>(m_playlist.size()) - 1);
        m_playlistIndex = dist(gen);
    } else {
        m_playlistIndex++;
        if (m_playlistIndex >= static_cast<int>(m_playlist.size())) {
            if (m_loopPlaylist) {
                m_playlistIndex = 0;
            } else {
                stop(1.0f);
                return;
            }
        }
    }

    crossfadeTo(m_playlist[m_playlistIndex], 1.0f);
}

void MusicManager::playPrevious() {
    if (m_playlist.empty()) {
        return;
    }

    m_playlistIndex--;
    if (m_playlistIndex < 0) {
        if (m_loopPlaylist) {
            m_playlistIndex = static_cast<int>(m_playlist.size()) - 1;
        } else {
            m_playlistIndex = 0;
        }
    }

    crossfadeTo(m_playlist[m_playlistIndex], 1.0f);
}

void MusicManager::setVolume(float volume) {
    m_volume = std::clamp(volume, 0.0f, 1.0f);
    m_targetVolume = m_volume;
    m_fadeSpeed = 0.0f;
    applyVolume();
}

void MusicManager::fadeVolumeTo(float targetVolume, float duration) {
    m_targetVolume = std::clamp(targetVolume, 0.0f, 1.0f);
    if (duration > 0.0f) {
        m_fadeSpeed = (m_targetVolume - m_volume) / duration;
    } else {
        m_volume = m_targetVolume;
        m_fadeSpeed = 0.0f;
        applyVolume();
    }
}

void* MusicManager::findTrack(MusicHandle handle) const {
    auto it = std::find_if(m_tracks.begin(), m_tracks.end(),
        [handle](const auto& pair) { return pair.first == handle; });

    return (it != m_tracks.end()) ? it->second : nullptr;
}

void MusicManager::applyVolume() {
#ifdef MCGNG_HAS_SDL2_MIXER
    int sdlVolume = static_cast<int>(m_volume * MIX_MAX_VOLUME);
    Mix_VolumeMusic(sdlVolume);
#endif
}

} // namespace mcgng
