#ifndef MCGNG_AUDIO_SYSTEM_H
#define MCGNG_AUDIO_SYSTEM_H

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

namespace mcgng {

/**
 * Sound handle type.
 */
using SoundHandle = uint32_t;
constexpr SoundHandle INVALID_SOUND = 0;

/**
 * Channel handle for playing sounds.
 */
using ChannelHandle = int;
constexpr ChannelHandle INVALID_CHANNEL = -1;

/**
 * Audio format.
 */
enum class AudioFormat {
    Unknown,
    WAV,
    OGG,
    MP3
};

/**
 * Sound instance for managing playback.
 */
struct SoundInstance {
    ChannelHandle channel = INVALID_CHANNEL;
    SoundHandle sound = INVALID_SOUND;
    bool looping = false;
    float volume = 1.0f;
    float pan = 0.0f;  // -1.0 (left) to 1.0 (right)
};

/**
 * Audio system for sound effects and music.
 *
 * Uses SDL2_mixer for audio playback.
 * Supports:
 * - WAV files (primary format for MCG audio)
 * - Multiple simultaneous sound channels
 * - Volume control per channel and global
 * - Stereo panning
 */
class AudioSystem {
public:
    static AudioSystem& instance();

    /**
     * Initialize the audio system.
     * @param frequency Audio frequency (default 44100)
     * @param channels Number of mix channels (default 16)
     * @return true on success
     */
    bool initialize(int frequency = 44100, int channels = 16);

    /**
     * Shutdown the audio system.
     */
    void shutdown();

    /**
     * Check if audio is initialized.
     */
    bool isInitialized() const { return m_initialized; }

    // Sound loading

    /**
     * Load a sound from file.
     * @param path Path to audio file
     * @return Sound handle or INVALID_SOUND on failure
     */
    SoundHandle loadSound(const std::string& path);

    /**
     * Load a sound from memory.
     * @param data Audio data
     * @param size Size of data
     * @param format Audio format
     * @return Sound handle or INVALID_SOUND on failure
     */
    SoundHandle loadSoundFromMemory(const uint8_t* data, size_t size, AudioFormat format);

    /**
     * Unload a sound.
     */
    void unloadSound(SoundHandle sound);

    /**
     * Unload all sounds.
     */
    void unloadAllSounds();

    // Sound playback

    /**
     * Play a sound.
     * @param sound Sound handle
     * @param loop Whether to loop
     * @param volume Volume (0.0 to 1.0)
     * @return Channel handle for controlling playback
     */
    ChannelHandle playSound(SoundHandle sound, bool loop = false, float volume = 1.0f);

    /**
     * Play a sound with position (for 3D audio approximation).
     * @param sound Sound handle
     * @param pan Stereo position (-1.0 left to 1.0 right)
     * @param volume Volume
     * @return Channel handle
     */
    ChannelHandle playSoundPanned(SoundHandle sound, float pan, float volume = 1.0f);

    /**
     * Stop a playing sound.
     */
    void stopChannel(ChannelHandle channel);

    /**
     * Stop all sounds.
     */
    void stopAllSounds();

    /**
     * Pause a channel.
     */
    void pauseChannel(ChannelHandle channel);

    /**
     * Resume a paused channel.
     */
    void resumeChannel(ChannelHandle channel);

    /**
     * Check if a channel is playing.
     */
    bool isChannelPlaying(ChannelHandle channel) const;

    /**
     * Set channel volume.
     */
    void setChannelVolume(ChannelHandle channel, float volume);

    /**
     * Set channel panning.
     */
    void setChannelPan(ChannelHandle channel, float pan);

    // Global controls

    /**
     * Set master volume (affects all sounds and music).
     */
    void setMasterVolume(float volume);
    float getMasterVolume() const { return m_masterVolume; }

    /**
     * Set sound effects volume.
     */
    void setSFXVolume(float volume);
    float getSFXVolume() const { return m_sfxVolume; }

    /**
     * Pause all audio.
     */
    void pauseAll();

    /**
     * Resume all audio.
     */
    void resumeAll();

    /**
     * Mute/unmute all audio.
     */
    void setMuted(bool muted);
    bool isMuted() const { return m_muted; }

private:
    AudioSystem() = default;
    ~AudioSystem();

    bool m_initialized = false;
    bool m_muted = false;

    float m_masterVolume = 1.0f;
    float m_sfxVolume = 1.0f;

    int m_numChannels = 16;

    // Sound storage (void* = Mix_Chunk*)
    std::unordered_map<SoundHandle, void*> m_sounds;
    SoundHandle m_nextSoundHandle = 1;

    int calculateVolume(float volume) const;
};

} // namespace mcgng

#endif // MCGNG_AUDIO_SYSTEM_H
