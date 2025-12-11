// ====================================================================
// STARFIELD EFFECT
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"
#include <cstdlib>
#include <cmath>

class EffectStars : public Effect {
public:
    std::string getName() const override { return "Starfield"; }
    std::string getDescription() const override {
        return "3D starfield with audio-reactive speed";
    }

    void init(int width, int height) override {
        Effect::init(width, height);
        for (int i = 0; i < NUM_STARS; i++) {
            m_stars[i][0] = (rand() % width) - width / 2;
            m_stars[i][1] = (rand() % height) - height / 2;
            m_stars[i][2] = 1 + rand() % 10;
        }
    }

    void reset() override {
        for (int i = 0; i < NUM_STARS; i++) {
            m_stars[i][0] = (rand() % m_width) - m_width / 2;
            m_stars[i][1] = (rand() % m_height) - m_height / 2;
            m_stars[i][2] = 1 + rand() % 10;
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

        float speed = 0.1f + vol * 0.5f;

        for (int i = 0; i < NUM_STARS; i++) {
            m_stars[i][2] -= speed;
            if (m_stars[i][2] <= 0) {
                m_stars[i][0] = (rand() % m_width) - m_width / 2;
                m_stars[i][1] = (rand() % m_height) - m_height / 2;
                m_stars[i][2] = 10;
            }

            // Project 3D to 2D
            float px = m_stars[i][0] / m_stars[i][2] * 20 + m_width / 2;
            float py = m_stars[i][1] / m_stars[i][2] * 20 + m_height / 2;

            if (px >= 0 && px < m_width && py >= 0 && py < m_height) {
                int intensity = (int)(settings.brightness * (10 - m_stars[i][2]) / 10);
                if (intensity > 255) intensity = 255;
                canvas->SetPixel((int)px, (int)py, intensity, intensity, intensity);
            }
        }
    }

private:
    static const int NUM_STARS = 64;
    float m_stars[NUM_STARS][3] = {0};
};
