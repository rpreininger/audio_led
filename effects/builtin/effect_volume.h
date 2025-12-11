// ====================================================================
// VOLUME BARS EFFECT
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"

class EffectVolume : public Effect {
public:
    std::string getName() const override { return "Volume Bars"; }
    std::string getDescription() const override {
        return "Green/cyan gradient bar responding to volume";
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        float vol = audio.volume;
        if (vol < settings.noiseThreshold) vol = 0;

        int h = static_cast<int>(vol * 80);
        if (h > m_height) h = m_height;

        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                if (h > 0 && y >= m_height - h) {
                    float f = static_cast<float>(y - (m_height - h)) / h;
                    canvas->SetPixel(x, y, 0,
                        static_cast<int>(settings.brightness * (1-f*0.5)),
                        static_cast<int>(255 * f));
                } else {
                    canvas->SetPixel(x, y, 0, 0, 0);
                }
            }
        }
    }
};
