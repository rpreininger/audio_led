// ====================================================================
// WEB SERVER - HTTP control interface
// ====================================================================
#pragma once

#include <atomic>
#include <thread>
#include <functional>
#include <string>
#include <vector>

struct WebSettings {
    std::atomic<int> brightness{180};
    std::atomic<float> sensitivity{100.0f};
    std::atomic<float> noiseThreshold{0.1f};
    std::atomic<int> effectDuration{5};
    std::atomic<int> currentEffect{-1};  // -1 = auto
    std::atomic<bool> autoLoop{true};
    std::atomic<bool> ftMode{false};     // false = audio visualizer, true = Flaschen-Taschen
    std::atomic<int> modeSpeed{4};       // seconds between Volume Bars mode changes
    std::atomic<int> animSpeed{100};     // animation speed percentage (10-200%)
};

class WebServer {
public:
    WebServer(int port = 8080);
    ~WebServer();

    // Start web server thread
    bool start();

    // Stop web server
    void stop();

    // Set effect names for UI
    void setEffectNames(const std::vector<std::string>& names);

    // Get settings reference
    WebSettings& getSettings() { return m_settings; }

    // Set callback for effect reload
    void setReloadCallback(std::function<void()> callback) {
        m_reloadCallback = callback;
    }

private:
    void serverThread();
    void handleClient(int clientSocket);
    std::string generateHTML();

    int m_port;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    WebSettings m_settings;
    std::vector<std::string> m_effectNames;
    std::function<void()> m_reloadCallback;
};
