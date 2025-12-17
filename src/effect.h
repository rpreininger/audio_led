// ====================================================================
// EFFECT BASE CLASS - Interface for all visual effects
// ====================================================================
#pragma once

#include <string>
#include <atomic>
#include <cstdint>
#include <cstring>

// Include LED matrix header for FrameCanvas
#include "led-matrix.h"

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

    // Get framebuffer for FT sending
    // Framebuffer format: RGB pixels, width * height * 3 bytes (row-major)
    virtual const uint8_t* getFramebuffer() const { return m_framebuffer; }

protected:
    int m_width = 128;
    int m_height = 64;

    // Framebuffer for FT capture (row-major: (y * width + x) * 3)
    mutable uint8_t m_framebuffer[128 * 64 * 3] = {0};

    // Helper to set pixel on canvas AND capture to framebuffer
    inline void setPixel(rgb_matrix::FrameCanvas* canvas, int x, int y,
                         uint8_t r, uint8_t g, uint8_t b) const {
        if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
            canvas->SetPixel(x, y, r, g, b);
            int idx = (y * m_width + x) * 3;
            m_framebuffer[idx] = r;
            m_framebuffer[idx + 1] = g;
            m_framebuffer[idx + 2] = b;
        }
    }

    // Helper to clear framebuffer
    void clearFramebuffer() const {
        memset(m_framebuffer, 0, sizeof(m_framebuffer));
    }

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
