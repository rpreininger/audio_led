// ====================================================================
// RAIN EFFECT
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"
#include <cstdlib>

class EffectRain : public Effect {
public:
    std::string getName() const override { return "Rain"; }
    std::string getDescription() const override {
        return "Falling raindrops with audio-reactive speed";
    }

    void init(int width, int height) override {
        Effect::init(width, height);
        for (int i = 0; i < NUM_DROPS; i++) {
            m_drops[i][0] = rand() % width;
            m_drops[i][1] = rand() % height;
        }
    }

    void reset() override {
        for (int i = 0; i < NUM_DROPS; i++) {
            m_drops[i][0] = rand() % m_width;
            m_drops[i][1] = rand() % m_height;
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
                setPixel(canvas,x, y, 0, 0, 0);
            }
        }

        // Move and draw drops
        float speed = 0.5f + vol * 2.0f;
        for (int i = 0; i < NUM_DROPS; i++) {
            m_drops[i][1] += speed;
            if (m_drops[i][1] >= m_height) {
                m_drops[i][1] = 0;
                m_drops[i][0] = rand() % m_width;
            }

            int x = (int)m_drops[i][0];
            int y = (int)m_drops[i][1];

            // Draw drop with tail
            for (int ty = 0; ty < 5; ty++) {
                int py = y - ty;
                if (py >= 0 && py < m_height) {
                    int intensity = settings.brightness * (5 - ty) / 5;
                    setPixel(canvas,x, py, 0, intensity / 2, intensity);
                }
            }
        }
    }

private:
    static const int NUM_DROPS = 32;
    float m_drops[NUM_DROPS][2] = {0};
};
