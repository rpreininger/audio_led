// ====================================================================
// MATRIX RAIN EFFECT - With falling characters
// ====================================================================
#pragma once

#include "../../src/effect.h"
#include "led-matrix.h"
#include <cstdlib>
#include <cmath>

class EffectMatrix : public Effect {
public:
    static constexpr int CHAR_WIDTH = 3;
    static constexpr int CHAR_HEIGHT = 5;
    static constexpr int NUM_GLYPHS = 40;
    static constexpr int MAX_COLUMNS = 40;  // 128 / (3+1) = 32 columns
    static constexpr int MAX_TRAIL = 12;    // Max characters in trail

    // 3x5 Matrix-style glyphs (katakana-inspired + numbers + symbols)
    // Each glyph is 5 rows of 3-bit patterns
    const uint8_t glyphs[NUM_GLYPHS][CHAR_HEIGHT] = {
        {0b010, 0b101, 0b111, 0b101, 0b101},  // A
        {0b111, 0b001, 0b010, 0b100, 0b111},  // Z
        {0b111, 0b100, 0b110, 0b100, 0b111},  // E
        {0b111, 0b100, 0b110, 0b100, 0b100},  // F
        {0b010, 0b101, 0b101, 0b101, 0b010},  // 0
        {0b010, 0b110, 0b010, 0b010, 0b111},  // 1
        {0b110, 0b001, 0b010, 0b100, 0b111},  // 2
        {0b110, 0b001, 0b010, 0b001, 0b110},  // 3
        {0b101, 0b101, 0b111, 0b001, 0b001},  // 4
        {0b111, 0b100, 0b110, 0b001, 0b110},  // 5
        {0b011, 0b100, 0b110, 0b101, 0b010},  // 6
        {0b111, 0b001, 0b010, 0b010, 0b010},  // 7
        {0b010, 0b101, 0b010, 0b101, 0b010},  // 8
        {0b010, 0b101, 0b011, 0b001, 0b110},  // 9
        {0b111, 0b010, 0b010, 0b010, 0b010},  // T (katakana ta)
        {0b010, 0b111, 0b010, 0b010, 0b100},  // katakana na
        {0b101, 0b111, 0b101, 0b001, 0b010},  // katakana ki
        {0b000, 0b111, 0b001, 0b001, 0b001},  // katakana ko
        {0b100, 0b111, 0b100, 0b100, 0b011},  // katakana ru
        {0b010, 0b000, 0b010, 0b000, 0b010},  // dots vertical
        {0b010, 0b010, 0b111, 0b010, 0b010},  // +
        {0b000, 0b000, 0b111, 0b000, 0b000},  // -
        {0b111, 0b101, 0b101, 0b101, 0b111},  // box
        {0b110, 0b110, 0b000, 0b110, 0b110},  // ::
        {0b001, 0b010, 0b100, 0b010, 0b001},  // >
        {0b100, 0b010, 0b001, 0b010, 0b100},  // <
        {0b010, 0b101, 0b010, 0b000, 0b000},  // ^
        {0b000, 0b000, 0b010, 0b101, 0b010},  // v
        {0b010, 0b111, 0b010, 0b000, 0b000},  // up arrow
        {0b000, 0b000, 0b010, 0b111, 0b010},  // down arrow
        {0b000, 0b010, 0b101, 0b010, 0b000},  // diamond
        {0b101, 0b010, 0b101, 0b000, 0b000},  // x small
        {0b101, 0b010, 0b010, 0b010, 0b101},  // X
        {0b111, 0b100, 0b100, 0b100, 0b111},  // C/bracket
        {0b111, 0b001, 0b001, 0b001, 0b111},  // reverse C
        {0b101, 0b101, 0b010, 0b010, 0b010},  // Y
        {0b101, 0b101, 0b111, 0b101, 0b101},  // H
        {0b001, 0b011, 0b111, 0b011, 0b001},  // arrow right
        {0b100, 0b110, 0b111, 0b110, 0b100},  // arrow left
        {0b010, 0b010, 0b010, 0b010, 0b010},  // |
    };

    std::string getName() const override { return "Matrix"; }
    std::string getDescription() const override {
        return "Matrix-style falling green characters";
    }

    void init(int width, int height) override {
        Effect::init(width, height);
        m_numColumns = width / (CHAR_WIDTH + 1);
        if (m_numColumns > MAX_COLUMNS) m_numColumns = MAX_COLUMNS;

        for (int i = 0; i < m_numColumns; i++) {
            resetColumn(i, true);
        }
    }

    void reset() override {
        for (int i = 0; i < m_numColumns; i++) {
            resetColumn(i, true);
        }
    }

    void resetColumn(int col, bool randomStart) {
        m_columnPos[col] = randomStart ? -(rand() % 15) - 1 : -(rand() % 5) - 1;
        m_speeds[col] = 0.08f + (rand() % 100) / 100.0f * 0.12f;  // Character cells per frame
        m_trailLength[col] = 4 + rand() % 6;  // 4 to 9 characters

        // Initialize trail with random glyphs
        for (int i = 0; i < MAX_TRAIL; i++) {
            m_trailGlyphs[col][i] = rand() % NUM_GLYPHS;
        }
    }

    void drawGlyph(rgb_matrix::FrameCanvas* canvas, int glyph, int x, int y, int r, int g, int b) {
        if (glyph < 0 || glyph >= NUM_GLYPHS) return;

        for (int row = 0; row < CHAR_HEIGHT; row++) {
            int py = y + row;
            if (py < 0 || py >= m_height) continue;

            uint8_t pattern = glyphs[glyph][row];
            for (int col = 0; col < CHAR_WIDTH; col++) {
                int px = x + col;
                if (px < 0 || px >= m_width) continue;

                // 3-bit pattern: check bits 2,1,0 from left to right
                if (pattern & (0b100 >> col)) {
                    setPixel(canvas, px, py, r, g, b);
                }
            }
        }
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        float vol = audio.volume;
        if (vol < settings.noiseThreshold) vol = 0;

        // Clear canvas
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                setPixel(canvas, x, y, 0, 0, 0);
            }
        }

        float baseSpeed = 1.0f + vol * 0.5f;
        float brightness = settings.brightness / 255.0f;

        for (int col = 0; col < m_numColumns; col++) {
            // Update position
            m_columnPos[col] += m_speeds[col] * baseSpeed;

            int headCharY = (int)m_columnPos[col];
            int trailLen = m_trailLength[col];
            int pixelX = col * (CHAR_WIDTH + 1);

            // Reset when trail exits screen
            int maxCharY = m_height / CHAR_HEIGHT;
            if (headCharY - trailLen > maxCharY) {
                resetColumn(col, false);
                continue;
            }

            // Occasionally change head glyph (the "flickering" effect)
            if (rand() % 8 == 0) {
                m_trailGlyphs[col][0] = rand() % NUM_GLYPHS;
            }

            // Draw each character in the trail
            for (int i = 0; i <= trailLen && i < MAX_TRAIL; i++) {
                int charY = headCharY - i;
                int pixelY = charY * CHAR_HEIGHT;

                if (pixelY + CHAR_HEIGHT < 0 || pixelY >= m_height) continue;

                int r, g, b;
                int glyph = m_trailGlyphs[col][i];

                if (i == 0) {
                    // Head: bright white-green
                    r = (int)(200 * brightness);
                    g = (int)(255 * brightness);
                    b = (int)(200 * brightness);
                } else if (i == 1) {
                    // Just behind head: bright green
                    r = (int)(30 * brightness);
                    g = (int)(255 * brightness);
                    b = (int)(30 * brightness);
                } else {
                    // Trail: fading green
                    float fade = 1.0f - ((float)(i - 1) / trailLen);
                    fade = fade * fade;  // Exponential falloff

                    r = 0;
                    g = (int)(160 * fade * brightness);
                    b = (int)(15 * fade * brightness);
                }

                drawGlyph(canvas, glyph, pixelX, pixelY, r, g, b);
            }

            // Shift trail glyphs down periodically (when head moves to new cell)
            static float lastPos[MAX_COLUMNS] = {0};
            if ((int)m_columnPos[col] > (int)lastPos[col]) {
                for (int i = MAX_TRAIL - 1; i > 0; i--) {
                    m_trailGlyphs[col][i] = m_trailGlyphs[col][i - 1];
                }
                m_trailGlyphs[col][0] = rand() % NUM_GLYPHS;
            }
            lastPos[col] = m_columnPos[col];
        }
    }

private:
    int m_numColumns = 0;
    float m_columnPos[MAX_COLUMNS] = {0};
    float m_speeds[MAX_COLUMNS] = {0};
    int m_trailLength[MAX_COLUMNS] = {0};
    int m_trailGlyphs[MAX_COLUMNS][MAX_TRAIL] = {{0}};
};
