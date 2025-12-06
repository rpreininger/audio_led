// ====================================================================
// SPECTRUM BARS EFFECT
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"

class EffectSpectrum : public Effect {
public:
    std::string getName() const override { return "Spectrum"; }
    std::string getDescription() const override {
        return "8-band FFT spectrum analyzer with rainbow colors";
    }

    void reset() override {
        for (int i = 0; i < 8; i++) m_smoothSpec[i] = 0;
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        const int bands = 8;
        int bw = m_width / bands;

        // Smooth spectrum values
        for (int i = 0; i < 8; i++) {
            float target = audio.spectrum[i];
            if (target > m_smoothSpec[i]) {
                m_smoothSpec[i] = target;
            } else {
                m_smoothSpec[i] = m_smoothSpec[i] * 0.85f + target * 0.15f;
            }
        }

        // Rainbow colors for each band
        static const int colors[8][3] = {
            {255, 0, 0},     // red
            {255, 128, 0},   // orange
            {255, 255, 0},   // yellow
            {0, 255, 0},     // green
            {0, 255, 255},   // cyan
            {0, 0, 255},     // blue
            {128, 0, 255},   // purple
            {255, 0, 255}    // magenta
        };

        for (int b = 0; b < bands; b++) {
            float val = m_smoothSpec[b];
            if (val < settings.noiseThreshold) val = 0;

            int h = static_cast<int>(val * 0.8f);
            if (h > m_height) h = m_height;

            int startX = b * bw + 2;
            int endX = (b + 1) * bw - 2;

            for (int y = 0; y < m_height; y++) {
                for (int x = startX; x < endX; x++) {
                    if (y >= m_height - h && h > 0) {
                        canvas->SetPixel(x, y,
                            colors[b][0], colors[b][1], colors[b][2]);
                    } else {
                        canvas->SetPixel(x, y, 0, 0, 0);
                    }
                }
            }

            // Black gaps between bars
            for (int y = 0; y < m_height; y++) {
                for (int x = b * bw; x < startX; x++) {
                    canvas->SetPixel(x, y, 0, 0, 0);
                }
                for (int x = endX; x < (b + 1) * bw; x++) {
                    canvas->SetPixel(x, y, 0, 0, 0);
                }
            }
        }
    }

private:
    float m_smoothSpec[8] = {0};
};
