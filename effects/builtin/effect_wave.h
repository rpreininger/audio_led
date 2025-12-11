// ====================================================================
// WAVEFORM EFFECT
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"
#include <cmath>

class EffectWave : public Effect {
public:
    std::string getName() const override { return "Waveform"; }
    std::string getDescription() const override {
        return "Audio-reactive waveform with color cycling";
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        float vol = audio.volume;
        float beat = audio.beat;
        if (vol < settings.noiseThreshold) vol = 0;

        // Clear
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                canvas->SetPixel(x, y, 0, 0, 0);
            }
        }

        int cy = m_height / 2;
        float baseAmplitude = vol * 28.0f;

        // Phase offset scrolls continuously
        m_phase += 0.05f + vol * 0.05f;

        // Slow color cycling
        m_hue += 0.002f;
        if (m_hue > 1.0f) m_hue -= 1.0f;

        int br = settings.brightness;

        for (int x = 0; x < m_width; x++) {
            // Create wave pattern
            float wave = sinf(x * 0.15f + m_phase) * 0.3f +
                         sinf(x * 0.08f - m_phase * 0.7f) * 0.2f +
                         0.5f;

            int amplitude = (int)(baseAmplitude * wave + beat * 8.0f);
            if (amplitude < 1 && vol > 0.05f) amplitude = 1;

            // HSV to RGB
            float h = m_hue + (float)x / m_width * 0.3f;
            if (h > 1.0f) h -= 1.0f;
            float hh = h * 6.0f;
            int i = (int)hh;
            float f = hh - i;
            float q = 1.0f - f;

            float rr, gg, bb;
            switch (i % 6) {
                case 0: rr = 1; gg = f; bb = 0; break;
                case 1: rr = q; gg = 1; bb = 0; break;
                case 2: rr = 0; gg = 1; bb = f; break;
                case 3: rr = 0; gg = q; bb = 1; break;
                case 4: rr = f; gg = 0; bb = 1; break;
                default: rr = 1; gg = 0; bb = q; break;
            }

            for (int dy = -amplitude; dy <= amplitude; dy++) {
                int y = cy + dy;
                if (y >= 0 && y < m_height) {
                    float dist = (float)abs(dy) / (amplitude + 1);
                    float intensity = (1 - dist);
                    int r = (int)(br * intensity * rr);
                    int g = (int)(br * intensity * gg);
                    int b = (int)(br * intensity * bb);
                    canvas->SetPixel(x, y, r, g, b);
                }
            }
        }
    }

private:
    float m_phase = 0;
    float m_hue = 0;
};
