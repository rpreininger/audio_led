// ====================================================================
// AUDIO CAPTURE - ALSA audio input with FFT analysis
// ====================================================================
#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include "effect.h"

class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    // Start audio capture thread
    bool start();

    // Stop audio capture
    void stop();

    // Get current audio data (thread-safe)
    AudioData getAudioData();

    // Set sensitivity multiplier
    void setSensitivity(float sensitivity);

    // Check if running
    bool isRunning() const { return m_running; }

private:
    void captureThread();

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<float> m_sensitivity{100.0f};

    // Audio state (protected by mutex)
    std::mutex m_mutex;
    AudioData m_audioData;

    // Beat detection state
    float m_lastEnergy{0};
    float m_beatSmooth{0};
};
