// ====================================================================
// FIRE EFFECT
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"
#include <cstdlib>

class EffectFire : public Effect {
public:
    std::string getName() const override { return "Fire"; }
    std::string getDescription() const override {
        return "Rising flames with heat from audio volume";
    }

    void init(int width, int height) override {
        Effect::init(width, height);
        // Initialize fire buffer
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 128; x++) {
                m_fire[y][x] = 0;
            }
        }
    }

    void reset() override {
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 128; x++) {
                m_fire[y][x] = 0;
            }
        }
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        // Shift upward
        for (int y = 0; y < m_height - 1; y++) {
            for (int x = 0; x < m_width; x++) {
                m_fire[y][x] = m_fire[y + 1][x];
            }
        }

        // Heat from audio volume
        float vol = audio.volume;
        if (vol < settings.noiseThreshold) vol = 0;
        int heat = static_cast<int>(vol * 300);
        if (heat > 255) heat = 255;

        // Add heat at bottom with randomness
        for (int x = 0; x < m_width; x++) {
            m_fire[m_height - 1][x] = heat + (rand() % 30) - 15;
            if (m_fire[m_height - 1][x] < 0) m_fire[m_height - 1][x] = 0;
            if (m_fire[m_height - 1][x] > 255) m_fire[m_height - 1][x] = 255;
        }

        // Blur and cool
        for (int y = 0; y < m_height - 1; y++) {
            for (int x = 0; x < m_width; x++) {
                int sum = m_fire[y][x];
                if (x > 0) sum += m_fire[y][x - 1];
                if (x < m_width - 1) sum += m_fire[y][x + 1];
                if (y < m_height - 1) sum += m_fire[y + 1][x];
                m_fire[y][x] = (sum / 4) - 2;
                if (m_fire[y][x] < 0) m_fire[y][x] = 0;
            }
        }

        // Draw with fire colors
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                int v = m_fire[y][x];
                canvas->SetPixel(x, y, v, v / 2, v / 8);
            }
        }
    }

private:
    int m_fire[64][128] = {0};
};
