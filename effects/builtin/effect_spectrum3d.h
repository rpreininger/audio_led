// ====================================================================
// SPECTRUM 3D WATERFALL EFFECT
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"

class EffectSpectrum3D : public Effect {
public:
    std::string getName() const override { return "Spectrum 3D"; }
    std::string getDescription() const override {
        return "3D waterfall spectrum analyzer";
    }

    void init(int width, int height) override {
        Effect::init(width, height);
        for (int d = 0; d < HISTORY_DEPTH; d++) {
            for (int b = 0; b < 8; b++) {
                m_history[d][b] = 0;
            }
        }
    }

    void reset() override {
        for (int d = 0; d < HISTORY_DEPTH; d++) {
            for (int b = 0; b < 8; b++) {
                m_history[d][b] = 0;
            }
        }
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        float threshold = settings.noiseThreshold;

        // Shift history every few frames
        m_frameCount++;
        if (m_frameCount >= 4) {
            m_frameCount = 0;
            for (int d = HISTORY_DEPTH - 1; d > 0; d--) {
                for (int b = 0; b < 8; b++) {
                    m_history[d][b] = m_history[d - 1][b];
                }
            }
            for (int b = 0; b < 8; b++) {
                float val = audio.spectrum[b];
                if (val < threshold) val = 0;
                m_history[0][b] = val;
            }
        }

        // Clear screen
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                setPixel(canvas,x, y, 0, 0, 0);
            }
        }

        int br = settings.brightness;

        // Draw 3D perspective lines - back to front
        for (int d = HISTORY_DEPTH - 1; d >= 0; d--) {
            float depthRatio = (float)d / HISTORY_DEPTH;

            int baseY = m_height - 8 - (int)(depthRatio * 50);
            float xScale = 1.0f - depthRatio * 0.5f;
            int xCenter = m_width / 2 + (int)(depthRatio * 20);
            float fade = 1.0f - depthRatio * 0.8f;

            if (baseY < 2) continue;

            int lineWidth = (int)(m_width * 0.8f * xScale);
            int startX = xCenter - lineWidth / 2;

            for (int x = 0; x < lineWidth; x++) {
                int px = startX + x;
                if (px < 0 || px >= m_width) continue;

                float bandPos = (float)x / lineWidth * 7.0f;
                int band1 = (int)bandPos;
                int band2 = band1 + 1;
                if (band2 > 7) band2 = 7;
                float frac = bandPos - band1;

                float val = m_history[d][band1] * (1.0f - frac) + m_history[d][band2] * frac;
                int h = (int)(val * 0.4f);
                if (h > 25) h = 25;

                // Rainbow color
                float hue = (float)x / lineWidth;
                float hh = hue * 6.0f;
                int i = (int)hh;
                float f = hh - i;
                float q = 1.0f - f;
                int r, g, bb;
                switch (i % 6) {
                    case 0: r = 255; g = (int)(f * 255); bb = 0; break;
                    case 1: r = (int)(q * 255); g = 255; bb = 0; break;
                    case 2: r = 0; g = 255; bb = (int)(f * 255); break;
                    case 3: r = 0; g = (int)(q * 255); bb = 255; break;
                    case 4: r = (int)(f * 255); g = 0; bb = 255; break;
                    case 5: r = 255; g = 0; bb = (int)(q * 255); break;
                    default: r = 255; g = 0; bb = 0; break;
                }

                r = (int)(r * fade * br / 255);
                g = (int)(g * fade * br / 255);
                bb = (int)(bb * fade * br / 255);

                int py = baseY - h;
                if (py >= 0 && py < m_height) {
                    setPixel(canvas,px, py, r, g, bb);
                }
            }
        }
    }

private:
    static const int HISTORY_DEPTH = 32;
    float m_history[HISTORY_DEPTH][8] = {0};
    int m_frameCount = 0;
};
