#include "audio/audio_system.h"
#include <iostream>
#include <algorithm>

#ifdef MCGNG_HAS_SDL2_MIXER
#include <SDL_mixer.h>
#endif

namespace mcgng {

AudioSystem& AudioSystem::instance() {
    static AudioSystem instance;
    return instance;
}

AudioSystem::~AudioSystem() {
    if (m_initialized) {
        shutdown();
    }
}

bool AudioSystem::initialize(int frequency, int channels) {
    if (m_initialized) {
        std::cerr << "AudioSystem: Already initialized\n";
        return false;
    }

    std::cout << "AudioSystem: Initializing (frequency=" << frequency
              << ", channels=" << channels << ")\n";

    m_numChannels = channels;

#ifdef MCGNG_HAS_SDL2_MIXER
    if (Mix_OpenAudio(frequency, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "AudioSystem: Failed to initialize SDL_mixer: " << Mix_GetError() << "\n";
        return false;
    }
    Mix_AllocateChannels(channels);
    std::cout << "AudioSystem: SDL2_mixer initialized successfully\n";
#else
    std::cout << "AudioSystem: SDL2_mixer not available (stub mode)\n";
#endif

    m_initialized = true;
    return true;
}

void AudioSystem::shutdown() {
    if (!m_initialized) {
        return;
    }

    std::cout << "AudioSystem: Shutting down\n";

    unloadAllSounds();

#ifdef MCGNG_HAS_SDL2_MIXER
    Mix_CloseAudio();
#endif

    m_initialized = false;
}

SoundHandle AudioSystem::loadSound(const std::string& path) {
    if (!m_initialized) {
        return INVALID_SOUND;
    }

    std::cout << "AudioSystem: Loading sound: " << path << "\n";

#ifdef MCGNG_HAS_SDL2_MIXER
    Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
    if (!chunk) {
        std::cerr << "AudioSystem: Failed to load sound: " << path << " - " << Mix_GetError() << "\n";
        return INVALID_SOUND;
    }

    SoundHandle handle = m_nextSoundHandle++;
    m_sounds[handle] = chunk;
    return handle;
#else
    SoundHandle handle = m_nextSoundHandle++;
    m_sounds[handle] = nullptr;
    return handle;
#endif
}

SoundHandle AudioSystem::loadSoundFromMemory(const uint8_t* data, size_t size, AudioFormat format) {
    if (!m_initialized || !data || size == 0) {
        return INVALID_SOUND;
    }

    (void)format;  // TODO: Handle different formats

#ifdef MCGNG_HAS_SDL2_MIXER
    SDL_RWops* rw = SDL_RWFromConstMem(data, static_cast<int>(size));
    if (!rw) {
        std::cerr << "AudioSystem: Failed to create RWops from memory\n";
        return INVALID_SOUND;
    }
    Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 1);  // 1 = free RWops after load
    if (!chunk) {
        std::cerr << "AudioSystem: Failed to load sound from memory: " << Mix_GetError() << "\n";
        return INVALID_SOUND;
    }

    SoundHandle handle = m_nextSoundHandle++;
    m_sounds[handle] = chunk;
    return handle;
#else
    SoundHandle handle = m_nextSoundHandle++;
    m_sounds[handle] = nullptr;
    return handle;
#endif
}

void AudioSystem::unloadSound(SoundHandle sound) {
    auto it = m_sounds.find(sound);
    if (it != m_sounds.end()) {
#ifdef MCGNG_HAS_SDL2_MIXER
        if (it->second) {
            Mix_FreeChunk(static_cast<Mix_Chunk*>(it->second));
        }
#endif
        m_sounds.erase(it);
    }
}

void AudioSystem::unloadAllSounds() {
#ifdef MCGNG_HAS_SDL2_MIXER
    for (auto& pair : m_sounds) {
        if (pair.second) {
            Mix_FreeChunk(static_cast<Mix_Chunk*>(pair.second));
        }
    }
#endif
    m_sounds.clear();
}

ChannelHandle AudioSystem::playSound(SoundHandle sound, bool loop, float volume) {
    if (!m_initialized || m_muted) {
        return INVALID_CHANNEL;
    }

    auto it = m_sounds.find(sound);
    if (it == m_sounds.end() || !it->second) {
        return INVALID_CHANNEL;
    }

    int sdlVolume = calculateVolume(volume);
    int loops = loop ? -1 : 0;

#ifdef MCGNG_HAS_SDL2_MIXER
    Mix_Chunk* chunk = static_cast<Mix_Chunk*>(it->second);
    Mix_VolumeChunk(chunk, sdlVolume);
    int channel = Mix_PlayChannel(-1, chunk, loops);
    if (channel == -1) {
        std::cerr << "AudioSystem: Failed to play sound: " << Mix_GetError() << "\n";
        return INVALID_CHANNEL;
    }
    return channel;
#else
    (void)sdlVolume;
    (void)loops;
    return 0;
#endif
}

ChannelHandle AudioSystem::playSoundPanned(SoundHandle sound, float pan, float volume) {
    ChannelHandle channel = playSound(sound, false, volume);
    if (channel != INVALID_CHANNEL) {
        setChannelPan(channel, pan);
    }
    return channel;
}

void AudioSystem::stopChannel(ChannelHandle channel) {
    if (!m_initialized || channel == INVALID_CHANNEL) {
        return;
    }

#ifdef MCGNG_HAS_SDL2_MIXER
    Mix_HaltChannel(channel);
#endif
}

void AudioSystem::stopAllSounds() {
    if (!m_initialized) {
        return;
    }

#ifdef MCGNG_HAS_SDL2_MIXER
    Mix_HaltChannel(-1);
#endif
}

void AudioSystem::pauseChannel(ChannelHandle channel) {
    if (!m_initialized || channel == INVALID_CHANNEL) {
        return;
    }

#ifdef MCGNG_HAS_SDL2_MIXER
    Mix_Pause(channel);
#endif
}

void AudioSystem::resumeChannel(ChannelHandle channel) {
    if (!m_initialized || channel == INVALID_CHANNEL) {
        return;
    }

#ifdef MCGNG_HAS_SDL2_MIXER
    Mix_Resume(channel);
#endif
}

bool AudioSystem::isChannelPlaying(ChannelHandle channel) const {
    if (!m_initialized || channel == INVALID_CHANNEL) {
        return false;
    }

#ifdef MCGNG_HAS_SDL2_MIXER
    return Mix_Playing(channel) != 0;
#else
    return false;
#endif
}

void AudioSystem::setChannelVolume(ChannelHandle channel, float volume) {
    if (!m_initialized || channel == INVALID_CHANNEL) {
        return;
    }

    int sdlVolume = calculateVolume(volume);

#ifdef MCGNG_HAS_SDL2_MIXER
    Mix_Volume(channel, sdlVolume);
#else
    (void)sdlVolume;
#endif
}

void AudioSystem::setChannelPan(ChannelHandle channel, float pan) {
    if (!m_initialized || channel == INVALID_CHANNEL) {
        return;
    }

    // Convert -1.0...1.0 to SDL2_mixer format
    pan = std::clamp(pan, -1.0f, 1.0f);
    uint8_t left = static_cast<uint8_t>((1.0f - pan) * 127.5f);
    uint8_t right = static_cast<uint8_t>((1.0f + pan) * 127.5f);

#ifdef MCGNG_HAS_SDL2_MIXER
    Mix_SetPanning(channel, left, right);
#else
    (void)left;
    (void)right;
#endif
}

void AudioSystem::setMasterVolume(float volume) {
    m_masterVolume = std::clamp(volume, 0.0f, 1.0f);

    // TODO: Update all channel volumes
}

void AudioSystem::setSFXVolume(float volume) {
    m_sfxVolume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioSystem::pauseAll() {
    if (!m_initialized) {
        return;
    }

#ifdef MCGNG_HAS_SDL2_MIXER
    Mix_Pause(-1);
#endif
}

void AudioSystem::resumeAll() {
    if (!m_initialized) {
        return;
    }

#ifdef MCGNG_HAS_SDL2_MIXER
    Mix_Resume(-1);
#endif
}

void AudioSystem::setMuted(bool muted) {
    m_muted = muted;

    if (muted) {
        pauseAll();
    } else {
        resumeAll();
    }
}

int AudioSystem::calculateVolume(float volume) const {
    // Convert 0.0-1.0 to 0-128 (MIX_MAX_VOLUME)
    float effectiveVolume = volume * m_sfxVolume * m_masterVolume;
    return static_cast<int>(std::clamp(effectiveVolume, 0.0f, 1.0f) * 128.0f);
}

} // namespace mcgng
