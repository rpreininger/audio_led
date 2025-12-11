// ====================================================================
// VU METER EFFECT
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"

class EffectVU : public Effect {
public:
    std::string getName() const override { return "VU Meter"; }
    std::string getDescription() const override {
        return "Stereo VU meter with peak hold";
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        float vol = audio.volume;
        if (vol < settings.noiseThreshold) vol = 0;

        // Simulate stereo with slight variation
        float left = vol * (0.9f + 0.2f * sin(vol * 10));
        float right = vol * (0.9f + 0.2f * cos(vol * 10));

        // Peak hold with decay
        if (left > m_peakL) m_peakL = left;
        else m_peakL *= 0.98f;
        if (right > m_peakR) m_peakR = right;
        else m_peakR *= 0.98f;

        int hL = (int)(left * 60);
        int hR = (int)(right * 60);
        int peakLy = (int)(m_peakL * 60);
        int peakRy = (int)(m_peakR * 60);

        if (hL > m_height) hL = m_height;
        if (hR > m_height) hR = m_height;
        if (peakLy > m_height) peakLy = m_height;
        if (peakRy > m_height) peakRy = m_height;

        int br = settings.brightness;

        // Clear
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                canvas->SetPixel(x, y, 0, 0, 0);
            }
        }

        // Draw left channel (0-63) - RED
        for (int y = m_height - hL; y < m_height; y++) {
            for (int x = 4; x < 60; x++) {
                float level = (float)(m_height - y) / m_height;
                int intensity = (int)(br * (0.5f + level * 0.5f));
                canvas->SetPixel(x, y, intensity, 0, 0);
            }
        }

        // Draw right channel (64-127) - GREEN
        for (int y = m_height - hR; y < m_height; y++) {
            for (int x = 68; x < 124; x++) {
                float level = (float)(m_height - y) / m_height;
                int intensity = (int)(br * (0.5f + level * 0.5f));
                canvas->SetPixel(x, y, 0, intensity, 0);
            }
        }

        // Peak indicators
        if (peakLy > 0) {
            int y = m_height - peakLy;
            for (int x = 4; x < 60; x++)
                canvas->SetPixel(x, y, br, br, br);
        }
        if (peakRy > 0) {
            int y = m_height - peakRy;
            for (int x = 68; x < 124; x++)
                canvas->SetPixel(x, y, br, br, br);
        }
    }

private:
    float m_peakL = 0;
    float m_peakR = 0;
};
