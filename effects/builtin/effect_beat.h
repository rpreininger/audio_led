// ====================================================================
// BEAT PULSE EFFECT
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"
#include <cmath>
#include <tuple>

class EffectBeat : public Effect {
public:
    std::string getName() const override { return "Beat Pulse"; }
    std::string getDescription() const override {
        return "Pulsing circle with beat detection and frequency wave";
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        float beat = audio.beat;
        float vol = audio.volume;
        float threshold = settings.noiseThreshold;

        // Slowly cycle hue
        m_hue += 0.003f;
        if (m_hue > 1.0f) m_hue -= 1.0f;

        // Clear screen
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                canvas->SetPixel(x, y, 0, 0, 0);
            }
        }

        if (vol < threshold && beat < threshold) {
            return;
        }

        // Radius based on beat and volume
        float radius = beat * 50.0f + vol * 30.0f;
        if (radius < 5.0f) return;
        if (radius > 70) radius = 70;

        int cx = m_width / 2;
        int cy = m_height / 2;

        // Draw circle with cycling color
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                float dx = x - cx;
                float dy = y - cy;
                float d = sqrt(dx * dx + dy * dy);

                if (d < radius) {
                    float f = 1.0f - d / radius;
                    auto [r, g, b] = hsvRgb(m_hue + d * 0.005f, settings.brightness * f);
                    canvas->SetPixel(x, y, r, g, b);
                }
            }
        }

        // Draw frequency wave line
        float lineHue = m_hue + 0.5f;
        if (lineHue > 1.0f) lineHue -= 1.0f;

        for (int x = 0; x < m_width; x++) {
            float bandPos = (float)x / m_width * 7.0f;
            int band1 = (int)bandPos;
            int band2 = band1 + 1;
            if (band2 > 7) band2 = 7;
            float frac = bandPos - band1;

            float val = audio.spectrum[band1] * (1.0f - frac) + audio.spectrum[band2] * frac;
            if (val < threshold) val = 0;

            int offset = (int)(val * 0.3f);
            if (offset > 20) offset = 20;

            auto [lr, lg, lb] = hsvRgb(lineHue + (float)x / m_width * 0.2f, settings.brightness);

            for (int dy = -1; dy <= 1; dy++) {
                int y = cy + offset + dy;
                if (y >= 0 && y < m_height) {
                    canvas->SetPixel(x, y, lr, lg, lb);
                }
                y = cy - offset + dy;
                if (y >= 0 && y < m_height) {
                    canvas->SetPixel(x, y, lr, lg, lb);
                }
            }
        }
    }

private:
    float m_hue = 0;

    std::tuple<int, int, int> hsvRgb(float h, float bright) {
        float hh = h * 6.0f;
        int i = (int)hh;
        float f = hh - i;
        float q = 1.0f - f;
        float r, g, b;
        switch (i % 6) {
            case 0: r = 1; g = f; b = 0; break;
            case 1: r = q; g = 1; b = 0; break;
            case 2: r = 0; g = 1; b = f; break;
            case 3: r = 0; g = q; b = 1; break;
            case 4: r = f; g = 0; b = 1; break;
            default: r = 1; g = 0; b = q; break;
        }
        return {(int)(r * bright), (int)(g * bright), (int)(b * bright)};
    }
};
