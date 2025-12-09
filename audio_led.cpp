// ====================================================================
//  AUDIO-LED VISUALIZER WITH WEB INTERFACE
//  Raspberry Pi Zero + 128x64 Panel
//  Hardware mapping: adafruit-hat-pwm
// ====================================================================

#include <alsa/asoundlib.h>
#include "kissfft/kiss_fft.h"

#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <iostream>
#include <sstream>
#include <tuple>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "led-matrix.h"
using namespace rgb_matrix;

// ====================================================================
// SETTINGS (adjustable via web)
// ====================================================================
static const int WIDTH  = 128;
static const int HEIGHT = 64;

struct Settings {
    std::atomic<int> effectDuration{5};      // seconds per effect
    std::atomic<int> brightness{180};         // 0-255
    std::atomic<float> noiseThreshold{0.1f}; // volume threshold
    std::atomic<int> currentEffect{-1};       // -1 = auto, 0-10 = manual
    std::atomic<float> sensitivity{4.0f};     // audio sensitivity multiplier (lower for line-in)
    std::atomic<bool> autoLoop{true};         // true = cycle through effects
    std::atomic<int> modeSpeed{4};            // seconds between Volume Bars mode changes
};

Settings settings;

// ====================================================================
// SHARED AUDIO STATE
// ====================================================================
struct AudioState {
    std::atomic<float> volume {0};
    std::atomic<float> beat {0};
    float spectrum[8] = {0};
    std::mutex specMutex;
};

AudioState audio;

// ====================================================================
// AUDIO THREAD (ALSA + FFT + BEAT DETECTION)
// ====================================================================
void audioThread() {

    snd_pcm_t* handle;
    int err;

    // Try different device names - plughw handles format conversion
    const char* devices[] = {
        "plughw:0,0",
        "plughw:1,0",
        "hw:0,0",
        "hw:1,0",
        "default",
        NULL
    };

    for (int i = 0; devices[i] != NULL; i++) {
        err = snd_pcm_open(&handle, devices[i], SND_PCM_STREAM_CAPTURE, 0);
        if (err >= 0) {
            std::cerr << "Opened audio device: " << devices[i] << "\n";
            break;
        }
    }

    if (err < 0) {
        std::cerr << "ALSA error: " << snd_strerror(err) << "\n";
        std::cerr << "Could not open any audio device\n";
        return;
    }

    err = snd_pcm_set_params(handle,
        SND_PCM_FORMAT_S16_LE,          // Format: 16-bit
        SND_PCM_ACCESS_RW_INTERLEAVED,  // Interleaved
        1,                              // Channels: 1 (Mono)
        44100,                          // Sample Rate
        1,                              // Allow resampling: yes
        500000);                        // Latency: 500ms
    if (err < 0) {
        std::cerr << "Set params error: " << snd_strerror(err) << "\n";
        // Try with different sample rate
        err = snd_pcm_set_params(handle,
            SND_PCM_FORMAT_S16_LE,
            SND_PCM_ACCESS_RW_INTERLEAVED,
            1,
            48000,                      // Try 48kHz
            1,
            500000);
        if (err < 0) {
            std::cerr << "Set params error (48kHz): " << snd_strerror(err) << "\n";
            return;
        }
        std::cerr << "Using 48kHz sample rate\n";
    } else {
        std::cerr << "Using 44.1kHz sample rate\n";
    }

    err = snd_pcm_prepare(handle);
    if (err < 0) {
        std::cerr << "Prepare error: " << snd_strerror(err) << "\n";
        return;
    }

    err = snd_pcm_start(handle);
    if (err < 0) {
        std::cerr << "Start error: " << snd_strerror(err) << "\n";
        return;
    }
    std::cerr << "PCM started successfully\n";

    const int N = 1024;
    int16_t buffer[N];  // 16-bit signed for S16_LE format
    float samples[N];   // normalized float samples

    kiss_fft_cfg cfg = kiss_fft_alloc(N, 0, NULL, NULL);
    kiss_fft_cpx in[N], out[N];

    float last_energy = 0;
    float beat_smooth = 0;

    std::cerr << "Audio capture started" << std::endl;
    int frameCount = 0;
    while (true) {
        frameCount++;
        // capture
        int frames = snd_pcm_readi(handle, buffer, N);

        if (frames < 0) {
            if (frames == -EPIPE) {
                // Overrun - need to prepare and restart
                snd_pcm_prepare(handle);
                snd_pcm_start(handle);
            } else if (frames == -EIO) {
                // I/O error - try full recovery
                snd_pcm_drop(handle);
                snd_pcm_prepare(handle);
                snd_pcm_start(handle);
            } else {
                snd_pcm_recover(handle, frames, 0);
            }
            continue;
        }

        // convert to normalized float
        for (int i = 0; i < N; i++)
            samples[i] = buffer[i] / 32768.0f;

        // VOLUME (scaled by sensitivity setting)
        float vol = 0;
        for (int i = 0; i < N; i++)
            vol += samples[i] * samples[i];
        vol = sqrt(vol / N) * settings.sensitivity.load();
        audio.volume.store(vol);

        // FFT
        for (int i = 0; i < N; i++) {
            in[i].r = samples[i];
            in[i].i = 0;
        }

        kiss_fft(cfg, in, out);

        // 8-band spectrum with logarithmic frequency bands
        // With 44.1kHz and N=1024: each bin = ~43Hz
        // Band boundaries designed for musical perception:
        // Band 0: Sub-bass     20-60 Hz    (bins 1-2)
        // Band 1: Bass         60-150 Hz   (bins 2-4)
        // Band 2: Low-mid      150-400 Hz  (bins 4-10)
        // Band 3: Mid          400-1kHz    (bins 10-24)
        // Band 4: Upper-mid    1-2.5kHz    (bins 24-58)
        // Band 5: Presence     2.5-5kHz    (bins 58-116)
        // Band 6: Brilliance   5-10kHz     (bins 116-232)
        // Band 7: Air          10-20kHz    (bins 232-465)
        {
            std::lock_guard<std::mutex> lock(audio.specMutex);

            // Logarithmic band boundaries (bin indices)
            const int bandStart[8] = {1,   2,   4,   10,  24,  58,  116, 232};
            const int bandEnd[8]   = {2,   4,   10,  24,  58,  116, 232, 465};

            // Per-band gain compensation (bass naturally has more energy)
            // Lower values = reduce gain, higher values = boost gain
            const float bandGain[8] = {0.3f, 0.5f, 0.8f, 1.0f, 1.5f, 2.5f, 4.0f, 6.0f};

            for (int b = 0; b < 8; b++) {
                float energy = 0;
                int start = bandStart[b];
                int end = bandEnd[b];
                if (end > N/2) end = N/2;  // Don't exceed Nyquist

                for (int i = start; i < end; i++)
                    energy += std::sqrt(out[i].r*out[i].r + out[i].i*out[i].i);

                int binCount = end - start;
                if (binCount < 1) binCount = 1;
                audio.spectrum[b] = (energy / binCount) * bandGain[b] * settings.sensitivity.load();
            }
        }

        // BEAT DETECTION - based on low frequency energy spikes
        float low = audio.spectrum[0] + audio.spectrum[1] + audio.spectrum[2];
        float diff = low - last_energy;
        last_energy = last_energy * 0.95f + low * 0.05f;  // slow moving average

        // Detect sudden increases in bass energy
        if (diff > 0.1f) {
            beat_smooth = 1.0f;  // immediate response on beat
        } else {
            beat_smooth = beat_smooth * 0.92f;  // decay
        }
        audio.beat.store(beat_smooth);

        // Debug every ~2 seconds (at ~43 fps audio = ~86 iterations)
        if (frameCount % 86 == 0) {
            std::cerr << "Vol: " << vol << " Beat: " << beat_smooth << std::endl;
        }
        // No sleep needed - snd_pcm_readi blocks until samples are ready
    }
}

// ====================================================================
// EFFECTS
// ====================================================================

// ---------------------- Volume Bars ------------------------------
void effect_volume(FrameCanvas *c, int br) {
    static int mode = 0;
    static float modeTimer = 0;
    static float hue = 0;
    static float particleX[16], particleY[16], particleVX[16], particleVY[16];
    static bool particlesInit = false;

    float vol = audio.volume.load();
    float beat = audio.beat.load();
    float threshold = settings.noiseThreshold.load();

    if (vol < threshold) vol = 0;

    // Change mode based on modeSpeed setting or on strong beat
    int modeSpeedSec = settings.modeSpeed.load();
    modeTimer += 0.016f;
    if (modeTimer > (float)modeSpeedSec || (beat > 0.8f && modeTimer > 1.0f)) {
        mode = (mode + 1) % 6;
        modeTimer = 0;
    }

    // Slowly rotate hue
    hue += 0.005f;
    if (hue > 1.0f) hue -= 1.0f;

    // HSV to RGB
    auto hsvRgb = [](float h, float bright) -> std::tuple<int,int,int> {
        float hh = h * 6.0f;
        int i = (int)hh;
        float f = hh - i;
        float q = 1.0f - f;
        float r, g, b;
        switch(i % 6) {
            case 0: r=1; g=f; b=0; break;
            case 1: r=q; g=1; b=0; break;
            case 2: r=0; g=1; b=f; break;
            case 3: r=0; g=q; b=1; break;
            case 4: r=f; g=0; b=1; break;
            default: r=1; g=0; b=q; break;
        }
        return {(int)(r*bright), (int)(g*bright), (int)(b*bright)};
    };

    // Clear screen
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            c->SetPixel(x, y, 0, 0, 0);

    int h = (int)(vol * 80);
    if (h > HEIGHT) h = HEIGHT;

    auto [cr, cg, cb] = hsvRgb(hue, br);

    switch(mode) {
        case 0: {
            // Centered expanding bars
            int barWidth = (int)(vol * 60) + 4;
            if (barWidth > WIDTH/2) barWidth = WIDTH/2;
            int cx = WIDTH/2;
            for (int y = HEIGHT - h; y < HEIGHT; y++) {
                float yf = (float)(y - (HEIGHT-h)) / (h > 0 ? h : 1);
                auto [r,g,b] = hsvRgb(hue + yf * 0.3f, br);
                for (int x = cx - barWidth; x < cx + barWidth; x++) {
                    if (x >= 0 && x < WIDTH)
                        c->SetPixel(x, y, r, g, b);
                }
            }
            break;
        }
        case 1: {
            // Rotating triangle
            static float angle = 0;
            angle += 0.03f + vol * 0.1f;  // Rotation speed based on volume

            int cx = WIDTH / 2;
            int cy = HEIGHT / 2;
            float size = 15.0f + vol * 40.0f;  // Triangle size based on volume

            // 3 triangle vertices
            float angles[3] = {angle, angle + 2.094f, angle + 4.189f};  // 120 degrees apart
            int px[3], py[3];
            for (int i = 0; i < 3; i++) {
                px[i] = cx + (int)(cos(angles[i]) * size);
                py[i] = cy + (int)(sin(angles[i]) * size * 0.5f);  // Squash for aspect ratio
            }

            // Draw filled triangle using scanline
            for (int y = 0; y < HEIGHT; y++) {
                for (int x = 0; x < WIDTH; x++) {
                    // Point-in-triangle test using barycentric coordinates
                    float d1 = (float)(x - px[1]) * (py[0] - py[1]) - (px[0] - px[1]) * (y - py[1]);
                    float d2 = (float)(x - px[2]) * (py[1] - py[2]) - (px[1] - px[2]) * (y - py[2]);
                    float d3 = (float)(x - px[0]) * (py[2] - py[0]) - (px[2] - px[0]) * (y - py[0]);

                    bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
                    bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

                    if (!(neg && pos)) {
                        // Inside triangle
                        float dx = x - cx, dy = y - cy;
                        float dist = sqrt(dx*dx + dy*dy);
                        float f = 1.0f - dist / (size + 1);
                        if (f < 0.3f) f = 0.3f;
                        auto [r,g,b] = hsvRgb(hue + dist * 0.01f, br * f);
                        c->SetPixel(x, y, r, g, b);
                    }
                }
            }
            break;
        }
        case 2: {
            // Diamond shape
            int size = (int)(vol * 50) + 5;
            int cx = WIDTH/2, cy = HEIGHT/2;
            for (int y = 0; y < HEIGHT; y++) {
                for (int x = 0; x < WIDTH; x++) {
                    int dist = abs(x - cx) + abs(y - cy);
                    if (dist < size) {
                        float f = 1.0f - (float)dist / size;
                        auto [r,g,b] = hsvRgb(hue + f * 0.2f, br * f);
                        c->SetPixel(x, y, r, g, b);
                    }
                }
            }
            break;
        }
        case 3: {
            // Horizontal mirrored bars (top and bottom)
            int barH = h / 2;
            for (int y = 0; y < barH; y++) {
                float yf = (float)y / (barH > 0 ? barH : 1);
                auto [r,g,b] = hsvRgb(hue + yf * 0.2f, br * (1.0f - yf * 0.5f));
                for (int x = 0; x < WIDTH; x++) {
                    c->SetPixel(x, y, r, g, b);
                    c->SetPixel(x, HEIGHT - 1 - y, r, g, b);
                }
            }
            break;
        }
        case 4: {
            // Corner triangles
            int size = (int)(vol * 60) + 5;
            for (int y = 0; y < HEIGHT; y++) {
                for (int x = 0; x < WIDTH; x++) {
                    bool inTri = (x + y < size) ||
                                 (x + (HEIGHT-y) < size) ||
                                 ((WIDTH-x) + y < size) ||
                                 ((WIDTH-x) + (HEIGHT-y) < size);
                    if (inTri) {
                        int d1 = x + y, d2 = x + (HEIGHT-y), d3 = (WIDTH-x) + y, d4 = (WIDTH-x) + (HEIGHT-y);
                        int dist = d1;
                        if (d2 < dist) dist = d2;
                        if (d3 < dist) dist = d3;
                        if (d4 < dist) dist = d4;
                        float f = 1.0f - (float)dist / size;
                        auto [r,g,b] = hsvRgb(hue + f * 0.3f, br * f);
                        c->SetPixel(x, y, r, g, b);
                    }
                }
            }
            break;
        }
        case 5: {
            // Concentric rings
            int cx = WIDTH/2, cy = HEIGHT/2;
            int maxRad = (int)(vol * 50) + 10;
            for (int y = 0; y < HEIGHT; y++) {
                for (int x = 0; x < WIDTH; x++) {
                    float dx = x - cx, dy = y - cy;
                    float dist = sqrt(dx*dx + dy*dy);
                    if (dist < maxRad) {
                        int ring = (int)(dist / 8);
                        if (ring % 2 == 0) {
                            float f = 1.0f - dist / maxRad;
                            auto [r,g,b] = hsvRgb(hue + ring * 0.15f, br * f);
                            c->SetPixel(x, y, r, g, b);
                        }
                    }
                }
            }
            break;
        }
    }
}

// ---------------------- Beat Pulse -------------------------------
void effect_beat(FrameCanvas *c, float t, int br) {
    static float hue = 0;

    float beat = audio.beat.load();
    float vol = audio.volume.load();
    float threshold = settings.noiseThreshold.load();

    // Slowly cycle hue
    hue += 0.003f;
    if (hue > 1.0f) hue -= 1.0f;

    // HSV to RGB helper
    auto hsvRgb = [](float h, float bright) -> std::tuple<int,int,int> {
        float hh = h * 6.0f;
        int i = (int)hh;
        float f = hh - i;
        float q = 1.0f - f;
        float r, g, b;
        switch(i % 6) {
            case 0: r=1; g=f; b=0; break;
            case 1: r=q; g=1; b=0; break;
            case 2: r=0; g=1; b=f; break;
            case 3: r=0; g=q; b=1; break;
            case 4: r=f; g=0; b=1; break;
            default: r=1; g=0; b=q; break;
        }
        return {(int)(r*bright), (int)(g*bright), (int)(b*bright)};
    };

    // Clear entire screen to black first
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            c->SetPixel(x, y, 0, 0, 0);
        }
    }

    // Only draw if above noise threshold
    if (vol < threshold && beat < threshold) {
        return;  // stay black
    }

    // Radius based on beat and volume
    float radius = beat * 50.0f + vol * 30.0f;
    if (radius < 5.0f) return;
    if (radius > 70) radius = 70;

    int cx = WIDTH/2;
    int cy = HEIGHT/2;

    // Draw circle with cycling color
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            float dx = x - cx;
            float dy = y - cy;
            float d  = sqrt(dx*dx + dy*dy);

            if (d < radius) {
                float f = 1.0f - d/radius;
                auto [r, g, b] = hsvRgb(hue + d * 0.005f, br * f);
                c->SetPixel(x, y, r, g, b);
            }
        }
    }

    // Draw frequency wave line through the middle
    float spec[8];
    {
        std::lock_guard<std::mutex> lock(audio.specMutex);
        for (int i = 0; i < 8; i++) spec[i] = audio.spectrum[i];
    }

    // Complementary color for line (opposite hue)
    float lineHue = hue + 0.5f;
    if (lineHue > 1.0f) lineHue -= 1.0f;

    // Draw a continuous wave line based on spectrum
    for (int x = 0; x < WIDTH; x++) {
        // Interpolate between spectrum bands
        float bandPos = (float)x / WIDTH * 7.0f;
        int band1 = (int)bandPos;
        int band2 = band1 + 1;
        if (band2 > 7) band2 = 7;
        float frac = bandPos - band1;

        float val = spec[band1] * (1.0f - frac) + spec[band2] * frac;
        if (val < threshold) val = 0;

        int offset = (int)(val * 0.3f);
        if (offset > 20) offset = 20;

        // Line color shifts slightly across width
        auto [lr, lg, lb] = hsvRgb(lineHue + (float)x / WIDTH * 0.2f, br);

        // Draw vertical line segment (wave thickness)
        for (int dy = -1; dy <= 1; dy++) {
            int y = cy + offset + dy;
            if (y >= 0 && y < HEIGHT) {
                c->SetPixel(x, y, lr, lg, lb);
            }
            y = cy - offset + dy;
            if (y >= 0 && y < HEIGHT) {
                c->SetPixel(x, y, lr, lg, lb);
            }
        }
    }
}

// Smoothed spectrum values (persistent)
static float smoothSpec[8] = {0};

// ---------------------- Spectrum Bars ----------------------------
void effect_spectrum(FrameCanvas *c, int br) {
    const int bands = 8;
    int bw = WIDTH / bands;

    // Copy and smooth spectrum values
    {
        std::lock_guard<std::mutex> lock(audio.specMutex);
        for (int i = 0; i < 8; i++) {
            float target = audio.spectrum[i];
            // Smooth: fast attack, slow decay
            if (target > smoothSpec[i]) {
                smoothSpec[i] = target;
            } else {
                smoothSpec[i] = smoothSpec[i] * 0.85f + target * 0.15f;
            }
        }
    }

    float threshold = settings.noiseThreshold.load();

    // Draw all pixels
    for (int b = 0; b < bands; b++) {
        float val = smoothSpec[b];

        // Noise threshold
        if (val < threshold) val = 0;

        int h = (int)(val * 0.8f);
        if (h > HEIGHT) h = HEIGHT;

        // Fixed colors for each band (rainbow)
        int r, g, bb;
        switch(b) {
            case 0: r = 255; g = 0;   bb = 0;   break;  // red
            case 1: r = 255; g = 128; bb = 0;   break;  // orange
            case 2: r = 255; g = 255; bb = 0;   break;  // yellow
            case 3: r = 0;   g = 255; bb = 0;   break;  // green
            case 4: r = 0;   g = 255; bb = 255; break;  // cyan
            case 5: r = 0;   g = 0;   bb = 255; break;  // blue
            case 6: r = 128; g = 0;   bb = 255; break;  // purple
            case 7: r = 255; g = 0;   bb = 255; break;  // magenta
        }

        int startX = b * bw + 2;
        int endX = (b + 1) * bw - 2;

        // Draw entire column - color below, black above
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = startX; x < endX; x++) {
                if (y >= HEIGHT - h && h > 0) {
                    c->SetPixel(x, y, r, g, bb);
                } else {
                    c->SetPixel(x, y, 0, 0, 0);
                }
            }
        }

        // Black gaps between bars
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = b * bw; x < startX; x++) {
                c->SetPixel(x, y, 0, 0, 0);
            }
            for (int x = endX; x < (b + 1) * bw; x++) {
                c->SetPixel(x, y, 0, 0, 0);
            }
        }
    }
}

// ---------------------- Plasma ----------------------------------
void effect_plasma(FrameCanvas *c, float t, int br) {
    float vol = audio.volume.load();
    float threshold = settings.noiseThreshold.load();
    if (vol < threshold) vol = 0;
    vol *= 6.0f;

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            float v = sin(x*0.09f + t)
                    + sin(y*0.08f + t*1.4f)
                    + sin((x+y)*0.04f + t*0.8f);

            int r = (int)((sin(v + t*0.5f + vol)*0.5f+0.5f) * br);
            int g = (int)((sin(v*1.3f + t + vol*0.5f)*0.5f+0.5f) * 255);
            int b = (int)((sin(v*2.3f + t*0.2f)*0.5f+0.5f) * 255);

            c->SetPixel(x, y, r, g, b);
        }
    }
}

// ---------------------- Fire -------------------------------------
void effect_fire(FrameCanvas *c, int br) {
    static int fire[HEIGHT][WIDTH] = {0};

    // shift upward
    for (int y = 0; y < HEIGHT-1; y++) {
        for (int x = 0; x < WIDTH; x++) {
            fire[y][x] = fire[y+1][x];
        }
    }

    // Heat from audio volume
    float vol = audio.volume.load();
    float threshold = settings.noiseThreshold.load();
    if (vol < threshold) vol = 0;
    int heat = (int)(vol * 300);
    if (heat > 255) heat = 255;

    // Add heat at bottom with some randomness
    for (int x = 0; x < WIDTH; x++) {
        fire[HEIGHT-1][x] = heat + (rand() % 30) - 15;
        if (fire[HEIGHT-1][x] < 0) fire[HEIGHT-1][x] = 0;
        if (fire[HEIGHT-1][x] > 255) fire[HEIGHT-1][x] = 255;
    }

    // blur/cool
    for (int y = 0; y < HEIGHT-1; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int sum = fire[y][x];
            if (x > 0) sum += fire[y][x-1];
            if (x < WIDTH-1) sum += fire[y][x+1];
            if (y < HEIGHT-1) sum += fire[y+1][x];
            fire[y][x] = (sum / 4) - 2;
            if (fire[y][x] < 0) fire[y][x] = 0;
        }
    }

    // draw with fire colors
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int v = fire[y][x];
            int r = v;
            int g = v / 2;
            int b = v / 8;
            c->SetPixel(x, y, r, g, b);
        }
    }
}

// ---------------------- Raindrops --------------------------------
void effect_rain(FrameCanvas *c, float t, int br) {
    static float drops[32][2];  // x, y positions
    static bool initialized = false;

    if (!initialized) {
        for (int i = 0; i < 32; i++) {
            drops[i][0] = rand() % WIDTH;
            drops[i][1] = rand() % HEIGHT;
        }
        initialized = true;
    }

    float vol = audio.volume.load();
    float threshold = settings.noiseThreshold.load();
    if (vol < threshold) vol = 0;

    // Clear
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            c->SetPixel(x, y, 0, 0, 0);

    // Move and draw drops
    float speed = 1.0f + vol * 5.0f;
    for (int i = 0; i < 32; i++) {
        drops[i][1] += speed;
        if (drops[i][1] >= HEIGHT) {
            drops[i][1] = 0;
            drops[i][0] = rand() % WIDTH;
        }

        int x = (int)drops[i][0];
        int y = (int)drops[i][1];

        // Draw drop with tail
        for (int ty = 0; ty < 5; ty++) {
            int py = y - ty;
            if (py >= 0 && py < HEIGHT) {
                int intensity = br * (5 - ty) / 5;
                c->SetPixel(x, py, 0, intensity / 2, intensity);
            }
        }
    }
}

// ---------------------- Matrix Rain ------------------------------
void effect_matrix(FrameCanvas *c, float t, int br) {
    static int columns[WIDTH];
    static int speeds[WIDTH];
    static bool initialized = false;

    if (!initialized) {
        for (int i = 0; i < WIDTH; i++) {
            columns[i] = rand() % HEIGHT;
            speeds[i] = 1 + rand() % 3;
        }
        initialized = true;
    }

    float vol = audio.volume.load();
    float threshold = settings.noiseThreshold.load();
    if (vol < threshold) vol = 0;

    // Fade existing pixels
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            c->SetPixel(x, y, 0, 0, 0);
        }
    }

    // Update and draw columns
    int activeSpeed = 1 + (int)(vol * 3);
    for (int x = 0; x < WIDTH; x += 2) {
        columns[x] += speeds[x] + activeSpeed - 1;
        if (columns[x] >= HEIGHT + 15) {
            columns[x] = 0;
            speeds[x] = 1 + rand() % 3;
        }

        // Draw falling trail
        for (int i = 0; i < 15; i++) {
            int y = columns[x] - i;
            if (y >= 0 && y < HEIGHT) {
                int g = br * (15 - i) / 15;
                c->SetPixel(x, y, g / 4, g, g / 4);
            }
        }
    }
}

// ---------------------- Starfield --------------------------------
void effect_stars(FrameCanvas *c, float t, int br) {
    static float stars[64][3];  // x, y, z
    static bool initialized = false;

    if (!initialized) {
        for (int i = 0; i < 64; i++) {
            stars[i][0] = (rand() % WIDTH) - WIDTH/2;
            stars[i][1] = (rand() % HEIGHT) - HEIGHT/2;
            stars[i][2] = 1 + rand() % 10;
        }
        initialized = true;
    }

    float vol = audio.volume.load();
    float threshold = settings.noiseThreshold.load();
    if (vol < threshold) vol = 0;

    // Clear
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            c->SetPixel(x, y, 0, 0, 0);

    float speed = 0.5f + vol * 2.0f;

    for (int i = 0; i < 64; i++) {
        stars[i][2] -= speed;
        if (stars[i][2] <= 0) {
            stars[i][0] = (rand() % WIDTH) - WIDTH/2;
            stars[i][1] = (rand() % HEIGHT) - HEIGHT/2;
            stars[i][2] = 10;
        }

        // Project 3D to 2D
        float px = stars[i][0] / stars[i][2] * 20 + WIDTH/2;
        float py = stars[i][1] / stars[i][2] * 20 + HEIGHT/2;

        if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
            int intensity = (int)(br * (10 - stars[i][2]) / 10);
            if (intensity > 255) intensity = 255;
            c->SetPixel((int)px, (int)py, intensity, intensity, intensity);
        }
    }
}

// ---------------------- VU Meter ---------------------------------
void effect_vu(FrameCanvas *c, int br) {
    static float peakL = 0, peakR = 0;

    float vol = audio.volume.load();
    float threshold = settings.noiseThreshold.load();
    if (vol < threshold) vol = 0;

    // Simulate stereo with slight variation
    float left = vol * (0.9f + 0.2f * sin(vol * 10));
    float right = vol * (0.9f + 0.2f * cos(vol * 10));

    // Peak hold with decay
    if (left > peakL) peakL = left;
    else peakL *= 0.98f;
    if (right > peakR) peakR = right;
    else peakR *= 0.98f;

    int hL = (int)(left * 60);
    int hR = (int)(right * 60);
    int peakLy = (int)(peakL * 60);
    int peakRy = (int)(peakR * 60);

    if (hL > HEIGHT) hL = HEIGHT;
    if (hR > HEIGHT) hR = HEIGHT;
    if (peakLy > HEIGHT) peakLy = HEIGHT;
    if (peakRy > HEIGHT) peakRy = HEIGHT;

    // Clear
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            c->SetPixel(x, y, 0, 0, 0);

    // Draw left channel (0-63) - RED
    for (int y = HEIGHT - hL; y < HEIGHT; y++) {
        for (int x = 4; x < 60; x++) {
            float level = (float)(HEIGHT - y) / HEIGHT;
            int intensity = (int)(br * (0.5f + level * 0.5f));
            c->SetPixel(x, y, intensity, 0, 0);
        }
    }

    // Draw right channel (64-127) - GREEN
    for (int y = HEIGHT - hR; y < HEIGHT; y++) {
        for (int x = 68; x < 124; x++) {
            float level = (float)(HEIGHT - y) / HEIGHT;
            int intensity = (int)(br * (0.5f + level * 0.5f));
            c->SetPixel(x, y, 0, intensity, 0);
        }
    }

    // Peak indicators
    if (peakLy > 0) {
        int y = HEIGHT - peakLy;
        for (int x = 4; x < 60; x++)
            c->SetPixel(x, y, br, br, br);
    }
    if (peakRy > 0) {
        int y = HEIGHT - peakRy;
        for (int x = 68; x < 124; x++)
            c->SetPixel(x, y, br, br, br);
    }
}

// ---------------------- Waveform ---------------------------------
void effect_wave(FrameCanvas *c, float t, int br) {
    static float history[WIDTH] = {0};

    float vol = audio.volume.load();
    float threshold = settings.noiseThreshold.load();
    if (vol < threshold) vol = 0;

    // Shift history left
    for (int i = 0; i < WIDTH - 1; i++)
        history[i] = history[i + 1];
    history[WIDTH - 1] = vol;

    // Clear
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            c->SetPixel(x, y, 0, 0, 0);

    // Draw waveform
    int cy = HEIGHT / 2;
    for (int x = 0; x < WIDTH; x++) {
        int amplitude = (int)(history[x] * 25);

        for (int dy = -amplitude; dy <= amplitude; dy++) {
            int y = cy + dy;
            if (y >= 0 && y < HEIGHT) {
                float dist = (float)abs(dy) / (amplitude + 1);
                int r = (int)(br * (1 - dist) * 0.5f);
                int g = (int)(br * (1 - dist));
                int b = (int)(255 * (1 - dist));
                c->SetPixel(x, y, r, g, b);
            }
        }
    }
}

// ---------------------- Color Pulse ------------------------------
void effect_colorpulse(FrameCanvas *c, float t, int br) {
    static float hue = 0;

    float vol = audio.volume.load();
    float threshold = settings.noiseThreshold.load();
    if (vol < threshold) vol = 0;

    // Slowly shift hue over time
    hue += 0.002f;
    if (hue > 1.0f) hue -= 1.0f;

    // Brightness based on volume
    float intensity = 0.1f + vol * 0.9f;
    if (intensity > 1.0f) intensity = 1.0f;

    // HSV to RGB conversion
    float h = hue * 6.0f;
    int i = (int)h;
    float f = h - i;
    float q = 1.0f - f;

    float r, g, b;
    switch (i % 6) {
        case 0: r = 1; g = f; b = 0; break;
        case 1: r = q; g = 1; b = 0; break;
        case 2: r = 0; g = 1; b = f; break;
        case 3: r = 0; g = q; b = 1; break;
        case 4: r = f; g = 0; b = 1; break;
        case 5: r = 1; g = 0; b = q; break;
        default: r = 1; g = 0; b = 0; break;
    }

    int pr = (int)(r * br * intensity);
    int pg = (int)(g * br * intensity);
    int pb = (int)(b * br * intensity);

    // Fill entire screen
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            c->SetPixel(x, y, pr, pg, pb);
        }
    }
}

// ---------------------- Color Wipe --------------------------------
void effect_colorwipe(FrameCanvas *c, float t, int br) {
    static float hue = 0;
    static float prevHue = 0;
    static int direction = 0;  // 0=left-right, 1=right-left, 2=top-bottom, 3=bottom-top
    static float wipeProgress = 0;

    float vol = audio.volume.load();
    float threshold = settings.noiseThreshold.load();
    if (vol < threshold) vol = 0;

    // Wipe speed based on volume
    float speed = 0.5f + vol * 2.0f;
    wipeProgress += speed;

    // Calculate wipe position
    int maxPos;
    if (direction == 0 || direction == 1) {
        maxPos = WIDTH;
    } else {
        maxPos = HEIGHT;
    }

    int wipePos = (int)wipeProgress;

    // When wipe completes, change direction and color
    if (wipePos >= maxPos) {
        wipeProgress = 0;
        prevHue = hue;
        hue += 0.15f;  // Jump to next color
        if (hue > 1.0f) hue -= 1.0f;
        direction = (direction + 1) % 4;  // Cycle through directions
    }

    // HSV to RGB for current color
    auto hsvToRgb = [](float h, int brightness) -> std::tuple<int, int, int> {
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
            case 5: r = 1; g = 0; b = q; break;
            default: r = 1; g = 0; b = 0; break;
        }
        return std::make_tuple((int)(r * brightness), (int)(g * brightness), (int)(b * brightness));
    };

    auto [nr, ng, nb] = hsvToRgb(hue, br);       // New color
    auto [pr, pg, pb] = hsvToRgb(prevHue, br);   // Previous color

    // Draw based on direction
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            bool useNewColor = false;

            switch (direction) {
                case 0:  // Left to right
                    useNewColor = (x < wipePos);
                    break;
                case 1:  // Right to left
                    useNewColor = (x >= WIDTH - wipePos);
                    break;
                case 2:  // Top to bottom
                    useNewColor = (y < wipePos);
                    break;
                case 3:  // Bottom to top
                    useNewColor = (y >= HEIGHT - wipePos);
                    break;
            }

            if (useNewColor) {
                c->SetPixel(x, y, nr, ng, nb);
            } else {
                c->SetPixel(x, y, pr, pg, pb);
            }
        }
    }
}

// ---------------------- Spectrum 3D Waterfall ------------------------
void effect_spectrum3d(FrameCanvas *c, float t, int br) {
    static const int HISTORY_DEPTH = 32;  // Number of history lines
    static float history[HISTORY_DEPTH][8] = {0};  // Store spectrum history
    static int frameCount = 0;

    // Get current spectrum
    float currentSpec[8];
    {
        std::lock_guard<std::mutex> lock(audio.specMutex);
        for (int i = 0; i < 8; i++) {
            currentSpec[i] = audio.spectrum[i];
        }
    }

    float threshold = settings.noiseThreshold.load();

    // Shift history back every few frames for slower movement
    frameCount++;
    if (frameCount >= 4) {
        frameCount = 0;
        for (int d = HISTORY_DEPTH - 1; d > 0; d--) {
            for (int b = 0; b < 8; b++) {
                history[d][b] = history[d-1][b];
            }
        }
        // Add new spectrum line at front
        for (int b = 0; b < 8; b++) {
            float val = currentSpec[b];
            if (val < threshold) val = 0;
            history[0][b] = val;
        }
    }

    // Clear screen
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            c->SetPixel(x, y, 0, 0, 0);
        }
    }

    // Draw 3D perspective lines - back to front so front overwrites
    for (int d = HISTORY_DEPTH - 1; d >= 0; d--) {
        float depthRatio = (float)d / HISTORY_DEPTH;

        // Perspective: lines move up and shrink horizontally as they go back
        int baseY = HEIGHT - 8 - (int)(depthRatio * 50);  // Move up with depth
        float xScale = 1.0f - depthRatio * 0.5f;  // Shrink width with depth
        int xCenter = WIDTH / 2 + (int)(depthRatio * 20);  // Shift right slightly
        float fade = 1.0f - depthRatio * 0.8f;  // Fade with depth

        if (baseY < 2) continue;

        // Calculate line width at this depth
        int lineWidth = (int)(WIDTH * 0.8f * xScale);
        int startX = xCenter - lineWidth / 2;

        // Draw horizontal line with height based on spectrum values
        for (int x = 0; x < lineWidth; x++) {
            int px = startX + x;
            if (px < 0 || px >= WIDTH) continue;

            // Map x position to spectrum band (interpolate between bands)
            float bandPos = (float)x / lineWidth * 7.0f;
            int band1 = (int)bandPos;
            int band2 = band1 + 1;
            if (band2 > 7) band2 = 7;
            float frac = bandPos - band1;

            // Interpolate between adjacent bands
            float val = history[d][band1] * (1.0f - frac) + history[d][band2] * frac;
            int h = (int)(val * 0.4f);
            if (h > 25) h = 25;

            // Color based on position (rainbow across width)
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

            // Apply brightness and depth fade
            r = (int)(r * fade * br / 255);
            g = (int)(g * fade * br / 255);
            bb = (int)(bb * fade * br / 255);

            // Draw the point at height offset from baseline
            int py = baseY - h;
            if (py >= 0 && py < HEIGHT) {
                c->SetPixel(px, py, r, g, bb);
            }
        }
    }
}

// ====================================================================
// EFFECT DISPATCHER
// ====================================================================
int autoEffect(float t) {
    int duration = settings.effectDuration.load();
    if (duration < 1) duration = 1;
    return ((int)(t / duration)) % 13; // 13 effects now
}

void renderEffect(int id, FrameCanvas *c, float t, int br) {
    switch(id) {
        case 0: effect_volume(c, br); break;
        case 1: effect_beat(c, t, br); break;
        case 2: effect_spectrum(c, br); break;
        case 3: effect_plasma(c, t, br); break;
        case 4: effect_fire(c, br); break;
        case 5: effect_rain(c, t, br); break;
        case 6: effect_matrix(c, t, br); break;
        case 7: effect_stars(c, t, br); break;
        case 8: effect_vu(c, br); break;
        case 9: effect_wave(c, t, br); break;
        case 10: effect_colorpulse(c, t, br); break;
        case 11: effect_colorwipe(c, t, br); break;
        case 12: effect_spectrum3d(c, t, br); break;
    }
}

// ====================================================================
// WEB SERVER
// ====================================================================
const char* HTML_PAGE = R"HTMLPAGE(
<!DOCTYPE html>
<html>
<head>
    <title>Audio LED Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; max-width: 600px; margin: 20px auto; padding: 10px; background: #1a1a2e; color: #eee; }
        h1 { color: #00d4ff; text-align: center; }
        .control { margin: 20px 0; padding: 15px; background: #16213e; border-radius: 10px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        input[type=range] { width: 100%; margin: 10px 0; }
        select { width: 100%; padding: 10px; font-size: 16px; background: #0f3460; color: #fff; border: none; border-radius: 5px; }
        .value { text-align: right; color: #00d4ff; font-size: 18px; }
        button { width: 100%; padding: 15px; font-size: 18px; background: #e94560; color: white; border: none; border-radius: 5px; cursor: pointer; margin-top: 20px; }
        button:hover { background: #ff6b6b; }
        .status { text-align: center; padding: 10px; background: #0f3460; border-radius: 5px; margin-top: 10px; }
    </style>
</head>
<body>
    <h1>Audio LED Control</h1>

    <div class="control">
        <label>Effect</label>
        <select id="effect" onchange="update()">
            <option value="-1">Auto (cycle)</option>
            <option value="0">Volume Bars</option>
            <option value="1">Beat Pulse</option>
            <option value="2">Spectrum</option>
            <option value="3">Plasma</option>
            <option value="4">Fire</option>
            <option value="5">Rain</option>
            <option value="6">Matrix</option>
            <option value="7">Starfield</option>
            <option value="8">VU Meter</option>
            <option value="9">Waveform</option>
            <option value="10">Color Pulse</option>
            <option value="11">Color Wipe</option>
            <option value="12">Spectrum 3D</option>
        </select>
    </div>

    <div class="control">
        <label>Brightness</label>
        <input type="range" id="brightness" min="10" max="255" value="180" oninput="update()">
        <div class="value" id="brightnessVal">180</div>
    </div>

    <div class="control">
        <label>Sensitivity</label>
        <input type="range" id="sensitivity" min="1" max="50" value="4" oninput="update()">
        <div class="value" id="sensitivityVal">4%</div>
    </div>

    <div class="control">
        <label>Noise Threshold</label>
        <input type="range" id="threshold" min="0" max="100" value="10" oninput="update()">
        <div class="value" id="thresholdVal">0.10</div>
    </div>

    <div class="control">
        <label>Effect Duration (seconds)</label>
        <input type="range" id="duration" min="2" max="60" value="5" oninput="update()">
        <div class="value" id="durationVal">5s</div>
    </div>

    <div class="control">
        <label>Mode Change Speed (seconds)</label>
        <input type="range" id="modespeed" min="1" max="30" value="4" oninput="update()">
        <div class="value" id="modespeedVal">4s</div>
    </div>

    <div class="control">
        <label style="display: inline;">Auto Loop Effects</label>
        <input type="checkbox" id="autoloop" checked onchange="update()" style="width: 24px; height: 24px; margin-left: 10px; vertical-align: middle;">
        <span id="autoloopStatus" style="margin-left: 10px; color: #00d4ff;">ON</span>
    </div>

    <div class="status" id="status">Ready</div>

    <script>
        function update() {
            var effect = document.getElementById("effect").value;
            var brightness = document.getElementById("brightness").value;
            var sensitivity = document.getElementById("sensitivity").value;
            var threshold = document.getElementById("threshold").value;
            var duration = document.getElementById("duration").value;
            var modespeed = document.getElementById("modespeed").value;
            var autoloop = document.getElementById("autoloop").checked ? 1 : 0;

            document.getElementById("brightnessVal").textContent = brightness;
            document.getElementById("sensitivityVal").textContent = sensitivity + "%";
            document.getElementById("thresholdVal").textContent = (threshold/100).toFixed(2);
            document.getElementById("durationVal").textContent = duration + "s";
            document.getElementById("modespeedVal").textContent = modespeed + "s";
            document.getElementById("autoloopStatus").textContent = autoloop ? "ON" : "OFF";

            fetch("/set?effect=" + effect + "&brightness=" + brightness +
                  "&sensitivity=" + sensitivity + "&threshold=" + threshold +
                  "&duration=" + duration + "&modespeed=" + modespeed + "&autoloop=" + autoloop)
                .then(r => r.text())
                .then(t => document.getElementById("status").textContent = t)
                .catch(e => document.getElementById("status").textContent = "Error: " + e);
        }

        // Load current values on page load
        fetch("/status")
            .then(r => r.json())
            .then(data => {
                document.getElementById("effect").value = data.effect;
                document.getElementById("brightness").value = data.brightness;
                document.getElementById("sensitivity").value = data.sensitivity;
                document.getElementById("threshold").value = data.threshold * 100;
                document.getElementById("duration").value = data.duration;
                document.getElementById("modespeed").value = data.modespeed;
                document.getElementById("autoloop").checked = data.autoloop;
                document.getElementById("brightnessVal").textContent = data.brightness;
                document.getElementById("sensitivityVal").textContent = data.sensitivity + "%";
                document.getElementById("thresholdVal").textContent = data.threshold.toFixed(2);
                document.getElementById("durationVal").textContent = data.duration + "s";
                document.getElementById("modespeedVal").textContent = data.modespeed + "s";
                document.getElementById("autoloopStatus").textContent = data.autoloop ? "ON" : "OFF";
            });
    </script>
</body>
</html>
)HTMLPAGE";

void handleClient(int clientSocket) {
    char buffer[2048] = {0};
    read(clientSocket, buffer, 2048);

    std::string request(buffer);
    std::string response;

    if (request.find("GET /set?") != std::string::npos) {
        // Parse parameters
        size_t pos;
        if ((pos = request.find("effect=")) != std::string::npos) {
            settings.currentEffect.store(atoi(request.c_str() + pos + 7));
        }
        if ((pos = request.find("brightness=")) != std::string::npos) {
            settings.brightness.store(atoi(request.c_str() + pos + 11));
        }
        if ((pos = request.find("sensitivity=")) != std::string::npos) {
            settings.sensitivity.store(atof(request.c_str() + pos + 12));
        }
        if ((pos = request.find("threshold=")) != std::string::npos) {
            settings.noiseThreshold.store(atof(request.c_str() + pos + 10) / 100.0f);
        }
        if ((pos = request.find("duration=")) != std::string::npos) {
            settings.effectDuration.store(atoi(request.c_str() + pos + 9));
        }
        if ((pos = request.find("modespeed=")) != std::string::npos) {
            settings.modeSpeed.store(atoi(request.c_str() + pos + 10));
        }
        if ((pos = request.find("autoloop=")) != std::string::npos) {
            settings.autoLoop.store(atoi(request.c_str() + pos + 9) != 0);
        }

        response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nSettings updated!";
    }
    else if (request.find("GET /status") != std::string::npos) {
        std::ostringstream json;
        json << "{\"effect\":" << settings.currentEffect.load()
             << ",\"brightness\":" << settings.brightness.load()
             << ",\"sensitivity\":" << settings.sensitivity.load()
             << ",\"threshold\":" << settings.noiseThreshold.load()
             << ",\"duration\":" << settings.effectDuration.load()
             << ",\"modespeed\":" << settings.modeSpeed.load()
             << ",\"autoloop\":" << (settings.autoLoop.load() ? "true" : "false") << "}";
        response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + json.str();
    }
    else {
        response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
        response += HTML_PAGE;
    }

    write(clientSocket, response.c_str(), response.length());
    close(clientSocket);
}

void webServerThread() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create web server socket\n";
        return;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    if (bind(serverSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind web server to port 8080\n";
        close(serverSocket);
        return;
    }

    listen(serverSocket, 5);
    std::cerr << "Web server running on http://0.0.0.0:8080\n";

    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket >= 0) {
            handleClient(clientSocket);
        }
    }
}

// ====================================================================
// MAIN
// ====================================================================
int main() {
    // LED INIT FIRST
    std::cerr << "Initializing LED matrix...\n";
    RGBMatrix::Options opt;
    opt.hardware_mapping = "adafruit-hat-pwm";
    opt.rows = 64;
    opt.cols = 128;
    opt.chain_length = 1;

    RuntimeOptions rt;
    rt.drop_privileges = 0;  // Keep root for audio access

    std::cerr << "Creating matrix...\n";
    RGBMatrix *matrix = CreateMatrixFromOptions(opt, rt);
    if (!matrix) {
        std::cerr << "Failed to create LED matrix\n";
        return 1;
    }
    std::cerr << "LED matrix initialized OK\n";

    FrameCanvas *canvas = matrix->CreateFrameCanvas();

    // START AUDIO THREAD AFTER LED INIT
    std::cerr << "Starting audio...\n";
    std::thread audioT(audioThread);
    audioT.detach();

    // START WEB SERVER
    std::cerr << "Starting web server...\n";
    std::thread webT(webServerThread);
    webT.detach();

    // Wait for audio to initialize
    std::this_thread::sleep_for(std::chrono::seconds(2));

    auto t0 = std::chrono::steady_clock::now();

    while (true) {
        float timeSec = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0
        ).count() / 1000.0f;

        // Get settings
        int br = settings.brightness.load();
        int manualEffect = settings.currentEffect.load();

        // Choose effect
        int id;
        bool loopEnabled = settings.autoLoop.load();
        if (manualEffect >= 0 && manualEffect <= 12) {
            // Manual effect selected - use it directly
            id = manualEffect;
        } else if (loopEnabled) {
            // Auto mode with loop enabled - cycle through effects
            id = autoEffect(timeSec);
        } else {
            // Auto mode with loop disabled - stay on effect 0
            id = 0;
        }

        renderEffect(id, canvas, timeSec, br);
        canvas = matrix->SwapOnVSync(canvas);

        // No sleep - VSync handles timing, maximizes refresh rate
    }
}
