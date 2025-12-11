// ====================================================================
//  AUDIO-LED VISUALIZER v2.0 - Plugin Architecture
//  Raspberry Pi Zero + 128x64 LED Panel
//  Features: Builtin C++ effects + Lua scripted effects
// ====================================================================

#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <dirent.h>
#include <mutex>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "led-matrix.h"
#include "effect.h"
#include "effect_manager.h"
#include "lua_effect.h"
#include "audio_capture.h"
#include "web_server.h"

// Builtin effects
#include "effect_volume.h"
#include "effect_spectrum.h"
#include "effect_plasma.h"
#include "effect_fire.h"
#include "effect_beat.h"
#include "effect_rain.h"
#include "effect_matrix.h"
#include "effect_stars.h"
#include "effect_vu.h"
#include "effect_wave.h"
#include "effect_colorpulse.h"
#include "effect_colorwipe.h"
#include "effect_spectrum3d.h"

using namespace rgb_matrix;

// Global for signal handling
static volatile bool g_running = true;

void signalHandler(int signum) {
    std::cerr << "\nShutting down..." << std::endl;
    g_running = false;
}

// ====================================================================
// FLASCHEN-TASCHEN SERVER (UDP PPM receiver on port 1337)
// ====================================================================
static const int FT_PORT = 1337;
static const int FT_WIDTH = 128;
static const int FT_HEIGHT = 64;
static uint8_t ftFramebuffer[FT_WIDTH * FT_HEIGHT * 3] = {0};
static std::mutex ftMutex;
static std::atomic<bool> ftHasNewFrame{false};

// Parse PPM P6 format from UDP packet
bool parsePPM(const char* data, int len, int& imgWidth, int& imgHeight, int& offsetX, int& offsetY) {
    if (len < 10) return false;
    if (data[0] != 'P' || data[1] != '6') return false;

    int pos = 2;
    while (pos < len && (data[pos] == ' ' || data[pos] == '\n' || data[pos] == '\r')) pos++;

    offsetX = 0;
    offsetY = 0;
    while (pos < len && data[pos] == '#') {
        if (pos + 4 < len && data[pos+1] == 'F' && data[pos+2] == 'T' && data[pos+3] == ':') {
            pos += 4;
            while (pos < len && data[pos] == ' ') pos++;
            offsetX = 0;
            while (pos < len && data[pos] >= '0' && data[pos] <= '9') {
                offsetX = offsetX * 10 + (data[pos] - '0');
                pos++;
            }
            while (pos < len && data[pos] == ' ') pos++;
            offsetY = 0;
            while (pos < len && data[pos] >= '0' && data[pos] <= '9') {
                offsetY = offsetY * 10 + (data[pos] - '0');
                pos++;
            }
            while (pos < len && data[pos] != '\n') pos++;
            if (pos < len) pos++;
        } else {
            while (pos < len && data[pos] != '\n') pos++;
            if (pos < len) pos++;
        }
    }

    imgWidth = 0;
    while (pos < len && data[pos] >= '0' && data[pos] <= '9') {
        imgWidth = imgWidth * 10 + (data[pos] - '0');
        pos++;
    }
    while (pos < len && (data[pos] == ' ' || data[pos] == '\n' || data[pos] == '\r')) pos++;

    imgHeight = 0;
    while (pos < len && data[pos] >= '0' && data[pos] <= '9') {
        imgHeight = imgHeight * 10 + (data[pos] - '0');
        pos++;
    }
    while (pos < len && (data[pos] == ' ' || data[pos] == '\n' || data[pos] == '\r')) pos++;

    int maxval = 0;
    while (pos < len && data[pos] >= '0' && data[pos] <= '9') {
        maxval = maxval * 10 + (data[pos] - '0');
        pos++;
    }
    if (pos < len && (data[pos] == ' ' || data[pos] == '\n' || data[pos] == '\r')) pos++;

    if (imgWidth <= 0 || imgHeight <= 0 || imgWidth > 1024 || imgHeight > 1024) return false;
    if (maxval != 255) return false;

    int expectedBytes = imgWidth * imgHeight * 3;
    if (pos + expectedBytes > len) return false;

    {
        std::lock_guard<std::mutex> lock(ftMutex);
        const uint8_t* pixelData = (const uint8_t*)(data + pos);

        for (int y = 0; y < imgHeight; y++) {
            int destY = offsetY + y;
            if (destY < 0 || destY >= FT_HEIGHT) continue;

            for (int x = 0; x < imgWidth; x++) {
                int destX = offsetX + x;
                if (destX < 0 || destX >= FT_WIDTH) continue;

                int srcIdx = (y * imgWidth + x) * 3;
                int dstIdx = (destY * FT_WIDTH + destX) * 3;

                ftFramebuffer[dstIdx + 0] = pixelData[srcIdx + 0];
                ftFramebuffer[dstIdx + 1] = pixelData[srcIdx + 1];
                ftFramebuffer[dstIdx + 2] = pixelData[srcIdx + 2];
            }
        }
    }
    ftHasNewFrame.store(true);
    return true;
}

void ftServerThread() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create FT UDP socket\n";
        return;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(FT_PORT);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind FT server to UDP port " << FT_PORT << "\n";
        close(sock);
        return;
    }

    std::cerr << "Flaschen-Taschen server listening on UDP port " << FT_PORT << "\n";

    char buffer[65536];
    while (g_running) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        int received = recvfrom(sock, buffer, sizeof(buffer), 0,
                                (struct sockaddr*)&clientAddr, &clientLen);
        if (received > 0) {
            int w, h, ox, oy;
            parsePPM(buffer, received, w, h, ox, oy);
        }
    }
    close(sock);
}

void renderFT(FrameCanvas* canvas) {
    std::lock_guard<std::mutex> lock(ftMutex);
    for (int y = 0; y < FT_HEIGHT; y++) {
        for (int x = 0; x < FT_WIDTH; x++) {
            int idx = (y * FT_WIDTH + x) * 3;
            canvas->SetPixel(x, y, ftFramebuffer[idx], ftFramebuffer[idx+1], ftFramebuffer[idx+2]);
        }
    }
}

// Load Lua effects from directory
void loadLuaEffects(EffectManager& manager, const std::string& scriptsDir) {
    std::cerr << "Looking for Lua scripts in: " << scriptsDir << std::endl;

    DIR* dir = opendir(scriptsDir.c_str());
    if (!dir) {
        std::cerr << "WARNING: Could not open scripts directory: " << scriptsDir << std::endl;
        std::cerr << "  Lua effects will not be available." << std::endl;
        std::cerr << "  Copy effects/scripts/ to the Pi or use --scripts <path>" << std::endl;
        return;
    }

    int luaCount = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        if (filename.length() > 4 &&
            filename.substr(filename.length() - 4) == ".lua") {
            std::string path = scriptsDir + "/" + filename;
            std::cerr << "  Loading: " << filename << std::endl;
            auto effect = std::make_unique<LuaEffect>(path);
            // init() is called in registerEffect(), which calls reload()
            // and sets m_valid. We register first, then check validity.
            effect->init(128, 64);  // Initialize to load the script
            if (effect->isValid()) {
                manager.registerEffect(std::move(effect));
                luaCount++;
            } else {
                std::cerr << "  FAILED: " << filename << std::endl;
            }
        }
    }
    closedir(dir);
    std::cerr << "Loaded " << luaCount << " Lua effects" << std::endl;
}

int main(int argc, char* argv[]) {
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Configuration
    const int WIDTH = 128;
    const int HEIGHT = 64;
    std::string scriptsDir = "./effects/scripts";

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--scripts" && i + 1 < argc) {
            scriptsDir = argv[++i];
        }
    }

    // ================================================================
    // Initialize LED Matrix
    // ================================================================
    std::cerr << "Initializing LED matrix..." << std::endl;

    RGBMatrix::Options opt;
    opt.hardware_mapping = "adafruit-hat-pwm";
    opt.rows = 64;
    opt.cols = 128;
    opt.chain_length = 1;

    RuntimeOptions rt;
    rt.drop_privileges = 0;  // Keep root for audio access

    RGBMatrix* matrix = CreateMatrixFromOptions(opt, rt);
    if (!matrix) {
        std::cerr << "Failed to create LED matrix" << std::endl;
        return 1;
    }
    std::cerr << "LED matrix initialized" << std::endl;

    FrameCanvas* canvas = matrix->CreateFrameCanvas();

    // ================================================================
    // Initialize Effect Manager
    // ================================================================
    std::cerr << "Loading effects..." << std::endl;

    EffectManager effectManager(WIDTH, HEIGHT);

    // Register builtin C++ effects
    effectManager.registerEffect(std::make_unique<EffectVolume>());
    effectManager.registerEffect(std::make_unique<EffectBeat>());
    effectManager.registerEffect(std::make_unique<EffectSpectrum>());
    effectManager.registerEffect(std::make_unique<EffectPlasma>());
    effectManager.registerEffect(std::make_unique<EffectFire>());
    effectManager.registerEffect(std::make_unique<EffectRain>());
    effectManager.registerEffect(std::make_unique<EffectMatrix>());
    effectManager.registerEffect(std::make_unique<EffectStars>());
    effectManager.registerEffect(std::make_unique<EffectVU>());
    effectManager.registerEffect(std::make_unique<EffectWave>());
    effectManager.registerEffect(std::make_unique<EffectColorPulse>());
    effectManager.registerEffect(std::make_unique<EffectColorWipe>());
    effectManager.registerEffect(std::make_unique<EffectSpectrum3D>());

    // Load Lua effects
    loadLuaEffects(effectManager, scriptsDir);

    std::cerr << "Loaded " << effectManager.getEffectCount() << " effects" << std::endl;

    // ================================================================
    // Initialize Audio Capture
    // ================================================================
    std::cerr << "Starting audio capture..." << std::endl;

    AudioCapture audio;
    if (!audio.start()) {
        std::cerr << "Warning: Audio capture failed to start" << std::endl;
    }

    // Wait for audio to initialize
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ================================================================
    // Initialize Web Server
    // ================================================================
    std::cerr << "Starting web server..." << std::endl;

    WebServer webServer(8080);
    webServer.setEffectNames(effectManager.getEffectNames());

    // Set reload callback for Lua hot-reload
    webServer.setReloadCallback([&]() {
        std::cerr << "Reloading Lua effects..." << std::endl;
        // Note: Full reload would require removing old Lua effects
        // and reloading them. For now, just reload existing ones.
        effectManager.reloadLuaEffects();
    });

    webServer.start();

    // ================================================================
    // Start Flaschen-Taschen Server
    // ================================================================
    std::cerr << "Starting Flaschen-Taschen server..." << std::endl;
    std::thread ftThread(ftServerThread);
    ftThread.detach();

    // ================================================================
    // Main Loop
    // ================================================================
    std::cerr << "Starting main loop..." << std::endl;

    auto t0 = std::chrono::steady_clock::now();
    int frameCount = 0;

    while (g_running) {
        float timeSec = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0
        ).count() / 1000.0f;

        // Get settings from web server
        WebSettings& settings = webServer.getSettings();
        int brightness = settings.brightness.load();
        bool ftModeActive = settings.ftMode.load();

        // Apply brightness
        matrix->SetBrightness(brightness * 100 / 255);

        Effect* effect = nullptr;

        if (ftModeActive) {
            // Flaschen-Taschen mode: render from UDP framebuffer
            renderFT(canvas);
        } else {
            // Audio visualizer mode
            int manualEffect = settings.currentEffect.load();
            bool autoLoop = settings.autoLoop.load();
            int duration = settings.effectDuration.load();

            // Update audio sensitivity
            audio.setSensitivity(settings.sensitivity.load());

            // Get audio data
            AudioData audioData = audio.getAudioData();

            // Build effect settings
            EffectSettings effectSettings;
            effectSettings.brightness = brightness;
            effectSettings.sensitivity = settings.sensitivity.load();
            effectSettings.noiseThreshold = settings.noiseThreshold.load();

            // Choose effect
            int effectId;
            int effectCount = effectManager.getEffectCount();

            if (manualEffect >= 0 && manualEffect < effectCount) {
                effectId = manualEffect;
            } else if (autoLoop && effectCount > 0) {
                if (duration < 1) duration = 1;
                effectId = ((int)(timeSec / duration)) % effectCount;
            } else {
                effectId = 0;
            }

            // Render effect
            effect = effectManager.getEffect(effectId);
            if (effect) {
                effect->update(canvas, audioData, effectSettings, timeSec);
            }
        }

        // Swap buffers
        canvas = matrix->SwapOnVSync(canvas);

        // Frame timing
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Debug output every 5 seconds
        frameCount++;
        if (frameCount % 500 == 0) {
            std::cerr << "FPS: ~" << (frameCount / timeSec)
                      << " Mode: " << (ftModeActive ? "FT" : (effect ? effect->getName() : "none"))
                      << std::endl;
        }
    }

    // ================================================================
    // Cleanup
    // ================================================================
    std::cerr << "Cleaning up..." << std::endl;

    webServer.stop();
    audio.stop();

    canvas->Clear();
    delete matrix;

    std::cerr << "Goodbye!" << std::endl;
    return 0;
}
