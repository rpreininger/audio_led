// ====================================================================
//  midi-ft-bridge
//  Receives USB MIDI events from Roland Fantom 06 and streams
//  mapped MP4 video clips to Flaschen-Taschen LED panels over WiFi
// ====================================================================

#include "config.h"
#include "midi_input.h"
#include "video_player.h"
#include "ft_sender.h"
#include "status_server.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <csignal>
#include <cstring>
#include <chrono>
#include <thread>
#include <atomic>
#include <functional>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

static std::atomic<bool> g_running{true};

static void signalHandler(int sig) {
    (void)sig;
    g_running = false;
}

// Active clip playback state per panel
struct ActiveClip {
    std::unique_ptr<VideoPlayer> player;
    std::string clipName;
    double fps;
    std::chrono::steady_clock::time_point lastFrameTime;
};

static void printUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --config <path>   Config file (default: config.json)\n"
              << "  --test            Keyboard test mode (F1-F12 trigger clips)\n"
              << "  --help            Show this help\n";
}

// Keyboard test mode: read F-keys from terminal in raw mode
// Returns the mapping index (0-11 for F1-F12), or -1 if no key pressed
static int pollKeyboard() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {0, 0}; // non-blocking

    if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0)
        return -1;

    char buf[8] = {0};
    int n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n < 1) return -1;

    // ESC sequence: F1=\e[OP or \eOP, F1-F4=\eOP..\eOS, F5-F12=\e[15~..\e[24~
    // Also handle number keys 1-9,0 as fallback
    if (n == 1) {
        // Number keys: '1'-'9' -> mapping 0-8, '0' -> mapping 9
        if (buf[0] >= '1' && buf[0] <= '9') return buf[0] - '1';
        if (buf[0] == '0') return 9;
        if (buf[0] == '-') return 10;
        if (buf[0] == '=') return 11;
        if (buf[0] == 'q' || buf[0] == 'Q') { g_running = false; return -1; }
        return -1;
    }

    // ESC [ sequences for F-keys
    if (buf[0] == '\x1b') {
        // \eOP..\eOS = F1-F4
        if (n >= 3 && buf[1] == 'O') {
            if (buf[2] == 'P') return 0;  // F1
            if (buf[2] == 'Q') return 1;  // F2
            if (buf[2] == 'R') return 2;  // F3
            if (buf[2] == 'S') return 3;  // F4
        }
        // \e[15~ = F5, \e[17~ = F6, \e[18~ = F7, \e[19~ = F8
        // \e[20~ = F9, \e[21~ = F10, \e[23~ = F11, \e[24~ = F12
        if (n >= 4 && buf[1] == '[' && buf[n-1] == '~') {
            int code = atoi(buf + 2);
            switch (code) {
                case 15: return 4;   // F5
                case 17: return 5;   // F6
                case 18: return 6;   // F7
                case 19: return 7;   // F8
                case 20: return 8;   // F9
                case 21: return 9;   // F10
                case 23: return 10;  // F11
                case 24: return 11;  // F12
            }
        }
    }
    return -1;
}

int main(int argc, char* argv[]) {
    std::string configPath = "config.json";
    bool testMode = false;

    // Parse CLI args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            configPath = argv[++i];
        } else if (strcmp(argv[i], "--test") == 0) {
            testMode = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }
    }

    // Install signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Load config
    Config config;
    if (!config.load(configPath)) {
        std::cerr << "Failed to load config from " << configPath << std::endl;
        return 1;
    }

    // Initialize FT senders (one per panel)
    std::vector<std::unique_ptr<FTSender>> senders;
    for (const auto& panel : config.panels) {
        auto sender = std::make_unique<FTSender>();
        if (!sender->init(panel.ip, panel.port)) {
            std::cerr << "Warning: Failed to init sender for panel "
                      << panel.name << " (" << panel.ip << ")" << std::endl;
        }
        senders.push_back(std::move(sender));
    }
    std::cerr << "Initialized " << senders.size() << " FT senders" << std::endl;

    // Build MIDI note -> mapping lookup
    // Key: MIDI note, Value: index into config.mappings
    std::map<int, int> noteMappings;
    for (size_t i = 0; i < config.mappings.size(); i++) {
        noteMappings[config.mappings[i].note] = (int)i;
    }

    // Active clips per panel (key: panel index)
    std::map<int, ActiveClip> activeClips;

    // Start MIDI input
    MidiInput midiInput;
    if (!midiInput.start()) {
        std::cerr << "Warning: MIDI input failed to start (continuing without MIDI)" << std::endl;
    }

    // Start status web server
    auto startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    StatusServer statusServer(config.web_port);
    statusServer.setConfig(&config);
    statusServer.setStartTime(startTime);
    if (midiInput.isRunning()) {
        statusServer.setMidiDevice(midiInput.getDeviceName());
    }

    statusServer.start();

    // Set up terminal raw mode for keyboard test mode
    struct termios oldTerm, newTerm;
    if (testMode) {
        tcgetattr(STDIN_FILENO, &oldTerm);
        newTerm = oldTerm;
        newTerm.c_lflag &= ~(ICANON | ECHO);
        newTerm.c_cc[VMIN] = 0;
        newTerm.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &newTerm);

        std::cerr << "\n=== KEYBOARD TEST MODE ===" << std::endl;
        std::cerr << "Keys 1-9, 0, -, = trigger clips (or F1-F12)" << std::endl;
        std::cerr << "Q = quit\n" << std::endl;
        for (size_t i = 0; i < config.mappings.size() && i < 12; i++) {
            const char* keys[] = {"1/F1","2/F2","3/F3","4/F4","5/F5","6/F6",
                                  "7/F7","8/F8","9/F9","0/F10","-/F11","=/F12"};
            std::cerr << "  " << keys[i] << " -> " << config.mappings[i].clip << std::endl;
        }
        std::cerr << std::endl;
    }

    std::cerr << "midi-ft-bridge running. Press Ctrl+C to stop." << std::endl;

    // Pre-allocate a black frame for clearing panels
    std::vector<uint8_t> blackFrame(config.width * config.height * 3, 0);

    // Helper: trigger a clip by mapping index
    std::function<void(int, std::chrono::steady_clock::time_point)> triggerMapping;
    triggerMapping = [&](int mappingIdx, std::chrono::steady_clock::time_point now) {
        if (mappingIdx < 0 || mappingIdx >= (int)config.mappings.size()) return;
        const auto& mapping = config.mappings[mappingIdx];
        std::string clipPath = config.clips_dir + "/" + mapping.clip;

        auto startClip = [&](int panelIdx) {
            auto player = std::make_unique<VideoPlayer>();
            if (player->open(clipPath, config.width, config.height)) {
                ActiveClip ac;
                ac.player = std::move(player);
                ac.clipName = mapping.clip;
                ac.fps = ac.player->getFPS();
                if (ac.fps <= 0) ac.fps = config.default_fps;
                ac.lastFrameTime = now;
                activeClips[panelIdx] = std::move(ac);
                std::cerr << "Playing " << mapping.clip << " on panel "
                          << config.panels[panelIdx].name << std::endl;
            }
        };

        if (mapping.panel == "all") {
            for (size_t i = 0; i < config.panels.size(); i++) {
                startClip((int)i);
            }
        } else {
            int idx = config.findPanel(mapping.panel);
            if (idx >= 0) startClip(idx);
        }
    };

    // Test callback: simulate a MIDI note press from the web UI
    statusServer.setTestCallback([&](int note) {
        auto it = noteMappings.find(note);
        if (it == noteMappings.end()) return;
        triggerMapping(it->second, std::chrono::steady_clock::now());
    });

    // Main loop
    while (g_running) {
        auto now = std::chrono::steady_clock::now();

        // Poll keyboard in test mode
        if (testMode) {
            int keyIdx = pollKeyboard();
            if (keyIdx >= 0) {
                triggerMapping(keyIdx, now);
            }
        }

        // Poll MIDI events
        MidiEvent event;
        while (midiInput.getNextEvent(event)) {
            // Log to status server
            MidiEventLog logEntry;
            logEntry.type = (event.type == MidiEvent::NOTE_ON) ? "NOTE_ON" : "NOTE_OFF";
            logEntry.note = event.note;
            logEntry.velocity = event.velocity;
            logEntry.channel = event.channel;
            logEntry.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            statusServer.logMidiEvent(logEntry);

            // Only handle Note On events on the configured channel
            if (event.type != MidiEvent::NOTE_ON) continue;
            if (config.midi_channel >= 0 && event.channel != config.midi_channel) continue;

            auto it = noteMappings.find(event.note);
            if (it == noteMappings.end()) continue;

            triggerMapping(it->second, now);
        }

        // Update active clips: decode next frame and send to panels
        std::vector<int> finishedPanels;

        for (auto& [panelIdx, clip] : activeClips) {
            double frameInterval = 1.0 / clip.fps;
            auto elapsed = std::chrono::duration<double>(now - clip.lastFrameTime).count();

            if (elapsed < frameInterval) continue;

            clip.lastFrameTime = now;

            const uint8_t* frame = clip.player->nextFrame();
            if (frame) {
                if (panelIdx >= 0 && panelIdx < (int)senders.size()) {
                    senders[panelIdx]->send(frame, config.width, config.height);
                }
            } else {
                // Clip finished - send black frame and mark for removal
                if (panelIdx >= 0 && panelIdx < (int)senders.size()) {
                    senders[panelIdx]->send(blackFrame.data(), config.width, config.height);
                }
                finishedPanels.push_back(panelIdx);
            }
        }

        for (int idx : finishedPanels) {
            std::cerr << "Clip finished on panel " << config.panels[idx].name << std::endl;
            activeClips.erase(idx);
        }

        // Update status server with panel info
        std::vector<PanelStatus> panelStatus;
        for (size_t i = 0; i < config.panels.size(); i++) {
            PanelStatus ps;
            ps.name = config.panels[i].name;
            ps.ip = config.panels[i].ip;
            ps.port = config.panels[i].port;
            ps.framesSent = (i < senders.size()) ? senders[i]->getFramesSent() : 0;
            ps.bytesSent = (i < senders.size()) ? senders[i]->getBytesSent() : 0;
            ps.enabled = (i < senders.size()) ? senders[i]->isEnabled() : false;

            auto it = activeClips.find((int)i);
            ps.activeClip = (it != activeClips.end()) ? it->second.clipName : "";

            panelStatus.push_back(ps);
        }
        statusServer.updatePanelStatus(panelStatus);

        // Update MIDI device name (may connect later)
        if (midiInput.isRunning()) {
            statusServer.setMidiDevice(midiInput.getDeviceName());
        }

        // Sleep to avoid busy-waiting (1ms resolution is fine for video playback)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Cleanup
    std::cerr << "\nShutting down..." << std::endl;

    // Restore terminal
    if (testMode) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldTerm);
    }

    // Send black to all panels
    for (size_t i = 0; i < senders.size(); i++) {
        senders[i]->send(blackFrame.data(), config.width, config.height);
    }

    statusServer.stop();
    midiInput.stop();

    std::cerr << "midi-ft-bridge stopped." << std::endl;
    return 0;
}
