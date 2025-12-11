// ====================================================================
// PLASMA EFFECT
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"
#include <cmath>

class EffectPlasma : public Effect {
public:
    std::string getName() const override { return "Plasma"; }
    std::string getDescription() const override {
        return "Animated plasma effect modulated by audio";
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        float vol = audio.volume;
        if (vol < settings.noiseThreshold) vol = 0;
        vol *= 6.0f;

        int br = settings.brightness;

        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                float v = sin(x * 0.09f + time)
                        + sin(y * 0.08f + time * 1.4f)
                        + sin((x + y) * 0.04f + time * 0.8f);

                int r = static_cast<int>((sin(v + time * 0.5f + vol) * 0.5f + 0.5f) * br);
                int g = static_cast<int>((sin(v * 1.3f + time + vol * 0.5f) * 0.5f + 0.5f) * 255);
                int b = static_cast<int>((sin(v * 2.3f + time * 0.2f) * 0.5f + 0.5f) * 255);

                canvas->SetPixel(x, y, r, g, b);
            }
        }
    }
};
