// ====================================================================
// COLOR PULSE EFFECT
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"

class EffectColorPulse : public Effect {
public:
    std::string getName() const override { return "Color Pulse"; }
    std::string getDescription() const override {
        return "Full-screen color cycling with volume intensity";
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        float vol = audio.volume;
        if (vol < settings.noiseThreshold) vol = 0;

        // Slowly shift hue
        m_hue += 0.002f;
        if (m_hue > 1.0f) m_hue -= 1.0f;

        // Brightness based on volume
        float intensity = 0.1f + vol * 0.9f;
        if (intensity > 1.0f) intensity = 1.0f;

        // HSV to RGB
        float h = m_hue * 6.0f;
        int i = (int)h;
        float f = h - i;
        float q = 1.0f - f;

        float r, g, b;
        switch (i % 6) {
            case 0: r = 1; g = f; b = 0; break;
            case 1: r = q; g = 1; b = 0; break;
            case 2: r = 0; g = 1; b = f; break;
            case 3: r = 0; g = q; b = 1; break;
            case 4: r = f; g = 0; b = 1; break;
            case 5: r = 1; g = 0; b = q; break;
            default: r = 1; g = 0; b = 0; break;
        }

        int pr = (int)(r * settings.brightness * intensity);
        int pg = (int)(g * settings.brightness * intensity);
        int pb = (int)(b * settings.brightness * intensity);

        // Fill entire screen
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                canvas->SetPixel(x, y, pr, pg, pb);
            }
        }
    }

private:
    float m_hue = 0;
};
