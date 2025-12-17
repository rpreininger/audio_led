// ====================================================================
// COLOR WIPE EFFECT
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"
#include <tuple>

class EffectColorWipe : public Effect {
public:
    std::string getName() const override { return "Color Wipe"; }
    std::string getDescription() const override {
        return "Directional color wipes with audio speed";
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        float vol = audio.volume;
        if (vol < settings.noiseThreshold) vol = 0;

        // Wipe speed based on volume
        float speed = 0.5f + vol * 2.0f;
        m_wipeProgress += speed;

        // Calculate wipe position
        int maxPos = (m_direction == 0 || m_direction == 1) ? m_width : m_height;
        int wipePos = (int)m_wipeProgress;

        // When wipe completes, change direction and color
        if (wipePos >= maxPos) {
            m_wipeProgress = 0;
            m_prevHue = m_hue;
            m_hue += 0.15f;
            if (m_hue > 1.0f) m_hue -= 1.0f;
            m_direction = (m_direction + 1) % 4;
        }

        auto [nr, ng, nb] = hsvToRgb(m_hue, settings.brightness);
        auto [pr, pg, pb] = hsvToRgb(m_prevHue, settings.brightness);

        // Draw based on direction
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                bool useNewColor = false;

                switch (m_direction) {
                    case 0: useNewColor = (x < wipePos); break;
                    case 1: useNewColor = (x >= m_width - wipePos); break;
                    case 2: useNewColor = (y < wipePos); break;
                    case 3: useNewColor = (y >= m_height - wipePos); break;
                }

                if (useNewColor) {
                    setPixel(canvas,x, y, nr, ng, nb);
                } else {
                    setPixel(canvas,x, y, pr, pg, pb);
                }
            }
        }
    }

private:
    float m_hue = 0;
    float m_prevHue = 0;
    int m_direction = 0;
    float m_wipeProgress = 0;

    std::tuple<int, int, int> hsvToRgb(float h, int brightness) {
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
            case 5: r = 1; g = 0; b = q; break;
            default: r = 1; g = 0; b = 0; break;
        }
        return {(int)(r * brightness), (int)(g * brightness), (int)(b * brightness)};
    }
};
