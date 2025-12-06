// ====================================================================
// EFFECT BASE CLASS - Interface for all visual effects
// ====================================================================
#pragma once

#include <string>
#include <atomic>

// Forward declaration
namespace rgb_matrix {
    class FrameCanvas;
}

// Audio data passed to effects
struct AudioData {
    float volume;       // Overall volume level
    float beat;         // Beat detection value
    float bass;         // Low frequency energy
    float mid;          // Mid frequency energy
    float treble;       // High frequency energy
    float spectrum[8];  // 8-band FFT spectrum
};

// Settings passed to effects
struct EffectSettings {
    int brightness;
    float sensitivity;
    float noiseThreshold;
};

// Base class for all effects
class Effect {
public:
    virtual ~Effect() = default;

    // Get effect name for UI
    virtual std::string getName() const = 0;

    // Get effect description
    virtual std::string getDescription() const { return ""; }

    // Initialize effect (called once when loaded)
    virtual void init(int width, int height) {
        m_width = width;
        m_height = height;
    }

    // Update and render the effect
    virtual void update(rgb_matrix::FrameCanvas* canvas,
                       const AudioData& audio,
                       const EffectSettings& settings,
                       float time) = 0;

    // Reset effect state
    virtual void reset() {}

protected:
    int m_width = 128;
    int m_height = 64;

    // Helper to clamp values
    static int clamp(int val, int min, int max) {
        if (val < min) return min;
        if (val > max) return max;
        return val;
    }

    static float clampf(float val, float min, float max) {
        if (val < min) return min;
        if (val > max) return max;
        return val;
    }
};
