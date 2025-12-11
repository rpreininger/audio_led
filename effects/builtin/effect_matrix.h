// ====================================================================
// MATRIX RAIN EFFECT
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"
#include <cstdlib>

class EffectMatrix : public Effect {
public:
    std::string getName() const override { return "Matrix"; }
    std::string getDescription() const override {
        return "Matrix-style falling green characters";
    }

    void init(int width, int height) override {
        Effect::init(width, height);
        for (int i = 0; i < 128; i++) {
            m_columns[i] = rand() % height;
            m_speeds[i] = 1 + rand() % 3;
            m_columnPos[i] = m_columns[i];
        }
    }

    void reset() override {
        for (int i = 0; i < 128; i++) {
            m_columns[i] = rand() % m_height;
            m_speeds[i] = 1 + rand() % 3;
            m_columnPos[i] = m_columns[i];
        }
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        float vol = audio.volume;
        if (vol < settings.noiseThreshold) vol = 0;

        // Clear
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                canvas->SetPixel(x, y, 0, 0, 0);
            }
        }

        // Update and draw columns
        float baseSpeed = 0.15f + vol * 0.5f;
        for (int x = 0; x < m_width; x += 2) {
            m_columnPos[x] += m_speeds[x] * 0.1f + baseSpeed;
            m_columns[x] = (int)m_columnPos[x];
            if (m_columns[x] >= m_height + 15) {
                m_columnPos[x] = 0;
                m_columns[x] = 0;
                m_speeds[x] = 1 + rand() % 3;
            }

            // Draw falling trail
            for (int i = 0; i < 15; i++) {
                int y = m_columns[x] - i;
                if (y >= 0 && y < m_height) {
                    int g = settings.brightness * (15 - i) / 15;
                    canvas->SetPixel(x, y, g / 4, g, g / 4);
                }
            }
        }
    }

private:
    int m_columns[128] = {0};
    int m_speeds[128] = {0};
    float m_columnPos[128] = {0};
};
