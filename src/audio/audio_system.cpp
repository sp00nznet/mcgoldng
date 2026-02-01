#include "audio/audio_system.h"
#include <iostream>
#include <algorithm>

// Note: Full implementation requires SDL2_mixer
// This is a stub that compiles without SDL2_mixer

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

    // TODO: Initialize SDL2_mixer
    // if (Mix_OpenAudio(frequency, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
    //     std::cerr << "AudioSystem: Failed to initialize SDL_mixer\n";
    //     return false;
    // }
    // Mix_AllocateChannels(channels);

    m_initialized = true;
    std::cout << "AudioSystem: Initialization complete (stub mode)\n";
    return true;
}

void AudioSystem::shutdown() {
    if (!m_initialized) {
        return;
    }

    std::cout << "AudioSystem: Shutting down\n";

    unloadAllSounds();

    // TODO: Close SDL2_mixer
    // Mix_CloseAudio();

    m_initialized = false;
}

SoundHandle AudioSystem::loadSound(const std::string& path) {
    if (!m_initialized) {
        return INVALID_SOUND;
    }

    std::cout << "AudioSystem: Loading sound: " << path << "\n";

    // TODO: Load sound with SDL2_mixer
    // Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
    // if (!chunk) {
    //     std::cerr << "AudioSystem: Failed to load sound: " << path << "\n";
    //     return INVALID_SOUND;
    // }

    SoundHandle handle = m_nextSoundHandle++;
    m_sounds[handle] = nullptr;  // Would store Mix_Chunk*

    return handle;
}

SoundHandle AudioSystem::loadSoundFromMemory(const uint8_t* data, size_t size, AudioFormat format) {
    if (!m_initialized || !data || size == 0) {
        return INVALID_SOUND;
    }

    (void)format;  // TODO: Handle different formats

    // TODO: Load from memory with SDL2_mixer
    // SDL_RWops* rw = SDL_RWFromConstMem(data, size);
    // Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 1);

    SoundHandle handle = m_nextSoundHandle++;
    m_sounds[handle] = nullptr;

    return handle;
}

void AudioSystem::unloadSound(SoundHandle sound) {
    auto it = m_sounds.find(sound);
    if (it != m_sounds.end()) {
        // TODO: Free sound with SDL2_mixer
        // Mix_FreeChunk(static_cast<Mix_Chunk*>(it->second));
        m_sounds.erase(it);
    }
}

void AudioSystem::unloadAllSounds() {
    // TODO: Free all sounds
    m_sounds.clear();
}

ChannelHandle AudioSystem::playSound(SoundHandle sound, bool loop, float volume) {
    if (!m_initialized || m_muted) {
        return INVALID_CHANNEL;
    }

    auto it = m_sounds.find(sound);
    if (it == m_sounds.end()) {
        return INVALID_CHANNEL;
    }

    int sdlVolume = calculateVolume(volume);
    int loops = loop ? -1 : 0;

    // TODO: Play with SDL2_mixer
    // Mix_VolumeChunk(static_cast<Mix_Chunk*>(it->second), sdlVolume);
    // int channel = Mix_PlayChannel(-1, static_cast<Mix_Chunk*>(it->second), loops);

    (void)sdlVolume;
    (void)loops;

    return 0;  // Return first channel as placeholder
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

    // TODO: Stop with SDL2_mixer
    // Mix_HaltChannel(channel);
}

void AudioSystem::stopAllSounds() {
    if (!m_initialized) {
        return;
    }

    // TODO: Stop all with SDL2_mixer
    // Mix_HaltChannel(-1);
}

void AudioSystem::pauseChannel(ChannelHandle channel) {
    if (!m_initialized || channel == INVALID_CHANNEL) {
        return;
    }

    // TODO: Pause with SDL2_mixer
    // Mix_Pause(channel);
}

void AudioSystem::resumeChannel(ChannelHandle channel) {
    if (!m_initialized || channel == INVALID_CHANNEL) {
        return;
    }

    // TODO: Resume with SDL2_mixer
    // Mix_Resume(channel);
}

bool AudioSystem::isChannelPlaying(ChannelHandle channel) const {
    if (!m_initialized || channel == INVALID_CHANNEL) {
        return false;
    }

    // TODO: Check with SDL2_mixer
    // return Mix_Playing(channel) != 0;

    return false;
}

void AudioSystem::setChannelVolume(ChannelHandle channel, float volume) {
    if (!m_initialized || channel == INVALID_CHANNEL) {
        return;
    }

    int sdlVolume = calculateVolume(volume);

    // TODO: Set volume with SDL2_mixer
    // Mix_Volume(channel, sdlVolume);
    (void)sdlVolume;
}

void AudioSystem::setChannelPan(ChannelHandle channel, float pan) {
    if (!m_initialized || channel == INVALID_CHANNEL) {
        return;
    }

    // Convert -1.0...1.0 to SDL2_mixer format
    // Left = 255, Right = 0 for SetPanning
    pan = std::clamp(pan, -1.0f, 1.0f);
    uint8_t left = static_cast<uint8_t>((1.0f - pan) * 127.5f);
    uint8_t right = static_cast<uint8_t>((1.0f + pan) * 127.5f);

    // TODO: Set panning with SDL2_mixer
    // Mix_SetPanning(channel, left, right);
    (void)left;
    (void)right;
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

    // TODO: Pause all with SDL2_mixer
    // Mix_Pause(-1);
}

void AudioSystem::resumeAll() {
    if (!m_initialized) {
        return;
    }

    // TODO: Resume all with SDL2_mixer
    // Mix_Resume(-1);
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
