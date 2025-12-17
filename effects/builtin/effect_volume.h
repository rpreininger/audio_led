// ====================================================================
// VOLUME BARS EFFECT - Multi-mode volume visualization
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"
#include <cmath>
#include <tuple>

class EffectVolume : public Effect {
public:
    std::string getName() const override { return "Volume Bars"; }
    std::string getDescription() const override {
        return "Multi-mode volume visualization with 6 variations";
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        float vol = audio.volume;
        float beat = audio.beat;
        if (vol < settings.noiseThreshold) vol = 0;

        // Change mode based on time (every ~4 seconds by default)
        // TODO: Use modeSpeed setting when available
        int newMode = ((int)(time / 4.0f)) % 6;
        if (newMode != m_mode) {
            m_mode = newMode;
        }

        // Slowly rotate hue
        m_hue += 0.005f;
        if (m_hue > 1.0f) m_hue -= 1.0f;

        // Clear screen
        for (int y = 0; y < m_height; y++)
            for (int x = 0; x < m_width; x++)
                setPixel(canvas,x, y, 0, 0, 0);

        int h = (int)(vol * 80);
        if (h > m_height) h = m_height;

        int br = settings.brightness;

        switch (m_mode) {
            case 0: renderCenteredBars(canvas, vol, h, br); break;
            case 1: renderRotatingTriangle(canvas, vol, time, br); break;
            case 2: renderDiamond(canvas, vol, br); break;
            case 3: renderMirroredBars(canvas, h, br); break;
            case 4: renderCornerTriangles(canvas, vol, br); break;
            case 5: renderConcentricRings(canvas, vol, br); break;
        }
    }

private:
    int m_mode = 0;
    float m_hue = 0;
    float m_angle = 0;

    std::tuple<int, int, int> hsvRgb(float h, float bright) {
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
            default: r = 1; g = 0; b = q; break;
        }
        return {(int)(r * bright), (int)(g * bright), (int)(b * bright)};
    }

    // Mode 0: Centered expanding bars
    void renderCenteredBars(rgb_matrix::FrameCanvas* canvas, float vol, int h, int br) {
        int barWidth = (int)(vol * 60) + 4;
        if (barWidth > m_width / 2) barWidth = m_width / 2;
        int cx = m_width / 2;

        for (int y = m_height - h; y < m_height; y++) {
            float yf = (float)(y - (m_height - h)) / (h > 0 ? h : 1);
            auto [r, g, b] = hsvRgb(m_hue + yf * 0.3f, br);
            for (int x = cx - barWidth; x < cx + barWidth; x++) {
                if (x >= 0 && x < m_width)
                    setPixel(canvas,x, y, r, g, b);
            }
        }
    }

    // Mode 1: Rotating triangle
    void renderRotatingTriangle(rgb_matrix::FrameCanvas* canvas, float vol, float time, int br) {
        m_angle += 0.05f + vol * 0.1f;

        int cx = m_width / 2;
        int cy = m_height / 2;
        float size = 15.0f + vol * 40.0f;

        float angles[3] = {m_angle, m_angle + 2.094f, m_angle + 4.189f};
        int px[3], py[3];
        for (int i = 0; i < 3; i++) {
            px[i] = cx + (int)(cos(angles[i]) * size);
            py[i] = cy + (int)(sin(angles[i]) * size * 0.5f);
        }

        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                float d1 = (float)(x - px[1]) * (py[0] - py[1]) - (px[0] - px[1]) * (y - py[1]);
                float d2 = (float)(x - px[2]) * (py[1] - py[2]) - (px[1] - px[2]) * (y - py[2]);
                float d3 = (float)(x - px[0]) * (py[2] - py[0]) - (px[2] - px[0]) * (y - py[0]);

                bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
                bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

                if (!(neg && pos)) {
                    float dx = x - cx, dy = y - cy;
                    float dist = sqrt(dx * dx + dy * dy);
                    float f = 1.0f - dist / (size + 1);
                    if (f < 0.3f) f = 0.3f;
                    auto [r, g, b] = hsvRgb(m_hue + dist * 0.01f, br * f);
                    setPixel(canvas,x, y, r, g, b);
                }
            }
        }
    }

    // Mode 2: Diamond shape
    void renderDiamond(rgb_matrix::FrameCanvas* canvas, float vol, int br) {
        int size = (int)(vol * 50) + 5;
        int cx = m_width / 2, cy = m_height / 2;

        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                int dist = abs(x - cx) + abs(y - cy);
                if (dist < size) {
                    float f = 1.0f - (float)dist / size;
                    auto [r, g, b] = hsvRgb(m_hue + f * 0.2f, br * f);
                    setPixel(canvas,x, y, r, g, b);
                }
            }
        }
    }

    // Mode 3: Horizontal mirrored bars
    void renderMirroredBars(rgb_matrix::FrameCanvas* canvas, int h, int br) {
        int barH = h / 2;

        for (int y = 0; y < barH; y++) {
            float yf = (float)y / (barH > 0 ? barH : 1);
            auto [r, g, b] = hsvRgb(m_hue + yf * 0.2f, br * (1.0f - yf * 0.5f));
            for (int x = 0; x < m_width; x++) {
                setPixel(canvas,x, y, r, g, b);
                setPixel(canvas,x, m_height - 1 - y, r, g, b);
            }
        }
    }

    // Mode 4: Corner triangles
    void renderCornerTriangles(rgb_matrix::FrameCanvas* canvas, float vol, int br) {
        int size = (int)(vol * 60) + 5;

        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                bool inTri = (x + y < size) ||
                             (x + (m_height - y) < size) ||
                             ((m_width - x) + y < size) ||
                             ((m_width - x) + (m_height - y) < size);
                if (inTri) {
                    int d1 = x + y;
                    int d2 = x + (m_height - y);
                    int d3 = (m_width - x) + y;
                    int d4 = (m_width - x) + (m_height - y);
                    int dist = d1;
                    if (d2 < dist) dist = d2;
                    if (d3 < dist) dist = d3;
                    if (d4 < dist) dist = d4;
                    float f = 1.0f - (float)dist / size;
                    auto [r, g, b] = hsvRgb(m_hue + f * 0.3f, br * f);
                    setPixel(canvas,x, y, r, g, b);
                }
            }
        }
    }

    // Mode 5: Concentric rings
    void renderConcentricRings(rgb_matrix::FrameCanvas* canvas, float vol, int br) {
        int cx = m_width / 2, cy = m_height / 2;
        int maxRad = (int)(vol * 50) + 10;

        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                float dx = x - cx, dy = y - cy;
                float dist = sqrt(dx * dx + dy * dy);
                if (dist < maxRad) {
                    int ring = (int)(dist / 8);
                    if (ring % 2 == 0) {
                        float f = 1.0f - dist / maxRad;
                        auto [r, g, b] = hsvRgb(m_hue + ring * 0.15f, br * f);
                        setPixel(canvas,x, y, r, g, b);
                    }
                }
            }
        }
    }
};
