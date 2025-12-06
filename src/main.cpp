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

using namespace rgb_matrix;

// Global for signal handling
static volatile bool g_running = true;

void signalHandler(int signum) {
    std::cerr << "\nShutting down..." << std::endl;
    g_running = false;
}

// Load Lua effects from directory
void loadLuaEffects(EffectManager& manager, const std::string& scriptsDir) {
    DIR* dir = opendir(scriptsDir.c_str());
    if (!dir) {
        std::cerr << "Could not open scripts directory: " << scriptsDir << std::endl;
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        if (filename.length() > 4 &&
            filename.substr(filename.length() - 4) == ".lua") {
            std::string path = scriptsDir + "/" + filename;
            auto effect = std::make_unique<LuaEffect>(path);
            if (effect->isValid()) {
                manager.registerEffect(std::move(effect));
            }
        }
    }
    closedir(dir);
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
    effectManager.registerEffect(std::make_unique<EffectSpectrum>());
    effectManager.registerEffect(std::make_unique<EffectPlasma>());
    effectManager.registerEffect(std::make_unique<EffectFire>());

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
        Effect* effect = effectManager.getEffect(effectId);
        if (effect) {
            effect->update(canvas, audioData, effectSettings, timeSec);
        }

        // Swap buffers
        canvas = matrix->SwapOnVSync(canvas);

        // Frame timing
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Debug output every 5 seconds
        frameCount++;
        if (frameCount % 500 == 0) {
            std::cerr << "FPS: ~" << (frameCount / timeSec)
                      << " Effect: " << (effect ? effect->getName() : "none")
                      << " Vol: " << audioData.volume << std::endl;
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
