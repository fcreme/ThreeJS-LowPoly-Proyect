#pragma once

#include <SDL.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace dw {

class AudioManager {
public:
    AudioManager() = default;
    ~AudioManager() = default;

    bool init();
    void shutdown();

    // Load a WAV file and store it by name
    bool loadSound(const std::string& name, const std::string& path);

    // Play a sound (one-shot, with optional volume and pitch)
    void playSound(const std::string& name, float volume = 1.0f, float pitch = 1.0f);

    // Play a sound with spatial positioning (volume attenuated by distance, stereo panned)
    void playSoundAt(const std::string& name, const glm::vec3& sourcePos);

    // Set listener position/orientation for spatial audio
    void setListenerPos(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& right);

    // Ambient loop (only one ambient at a time)
    bool loadAmbient(const std::string& path);
    void playAmbient();
    void stopAmbient();
    void setAmbientVolume(float vol); // 0..1

    // Footsteps
    void generateFootstepSound(); // procedural soft stone footstep
    void playFootstep();
    void setFootstepInterval(float seconds) { m_footstepInterval = seconds; }
    void updateFootsteps(float dt, bool isMoving);

    // Simple delay-line reverb
    void setReverb(bool enabled, float feedback = 0.3f, float delayMs = 500.0f);

private:
    static void audioCallback(void* userdata, Uint8* stream, int len);
    void mixAudio(Uint8* stream, int len);

    SDL_AudioDeviceID m_device = 0;
    SDL_AudioSpec m_spec{};

    struct SoundData {
        std::vector<Uint8> buffer;
        SDL_AudioSpec spec;
    };

    std::unordered_map<std::string, SoundData> m_sounds;

    // Ambient
    std::vector<Uint8> m_ambientBuffer;
    size_t m_ambientPos = 0;
    bool m_ambientPlaying = false;
    float m_ambientVolume = 0.3f;

    // One-shot playback tracking
    struct PlayingSound {
        const std::vector<Uint8>* buffer;
        size_t pos;
        float floatPos = 0.0f;   // fractional position for pitch-shifted playback
        float volumeL = 1.0f;    // left channel volume (for spatial)
        float volumeR = 1.0f;    // right channel volume
        float playbackRate = 1.0f; // pitch variation (1.0 = normal)
    };
    std::vector<PlayingSound> m_playing;

    // Listener state (for spatial audio)
    glm::vec3 m_listenerPos{0.0f};
    glm::vec3 m_listenerForward{0.0f, 0.0f, -1.0f};
    glm::vec3 m_listenerRight{1.0f, 0.0f, 0.0f};

    // Footsteps
    float m_footstepTimer = 0.0f;
    float m_footstepInterval = 0.5f;

    // Reverb delay line
    bool m_reverbEnabled = false;
    float m_reverbFeedback = 0.3f;
    std::vector<float> m_reverbBuffer;
    size_t m_reverbPos = 0;
    size_t m_reverbDelaySamples = 22050; // 0.5s at 44.1kHz
};

} // namespace dw
