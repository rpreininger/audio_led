// ====================================================================
// WEB SERVER - Implementation
// ====================================================================
#include "web_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iostream>

WebServer::WebServer(int port) : m_port(port) {}

WebServer::~WebServer() {
    stop();
}

bool WebServer::start() {
    if (m_running) return true;
    m_running = true;
    m_thread = std::thread(&WebServer::serverThread, this);
    return true;
}

void WebServer::stop() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void WebServer::setEffectNames(const std::vector<std::string>& names) {
    m_effectNames = names;
}

void WebServer::serverThread() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create web server socket" << std::endl;
        m_running = false;
        return;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (bind(serverSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind web server to port " << m_port << std::endl;
        close(serverSocket);
        m_running = false;
        return;
    }

    listen(serverSocket, 5);
    std::cerr << "Web server running on http://0.0.0.0:" << m_port << std::endl;

    while (m_running) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        // Use timeout to allow checking m_running
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket >= 0) {
            handleClient(clientSocket);
        }
    }

    close(serverSocket);
    std::cerr << "Web server stopped" << std::endl;
}

void WebServer::handleClient(int clientSocket) {
    char buffer[4096] = {0};
    read(clientSocket, buffer, sizeof(buffer) - 1);

    std::string request(buffer);
    std::string response;

    if (request.find("GET /set?") != std::string::npos) {
        // Parse parameters
        size_t pos;
        if ((pos = request.find("effect=")) != std::string::npos) {
            m_settings.currentEffect.store(atoi(request.c_str() + pos + 7));
        }
        if ((pos = request.find("brightness=")) != std::string::npos) {
            m_settings.brightness.store(atoi(request.c_str() + pos + 11));
        }
        if ((pos = request.find("sensitivity=")) != std::string::npos) {
            m_settings.sensitivity.store(atof(request.c_str() + pos + 12));
        }
        if ((pos = request.find("threshold=")) != std::string::npos) {
            m_settings.noiseThreshold.store(atof(request.c_str() + pos + 10) / 100.0f);
        }
        if ((pos = request.find("duration=")) != std::string::npos) {
            m_settings.effectDuration.store(atoi(request.c_str() + pos + 9));
        }
        if ((pos = request.find("autoloop=")) != std::string::npos) {
            m_settings.autoLoop.store(atoi(request.c_str() + pos + 9) != 0);
        }
        if ((pos = request.find("ftmode=")) != std::string::npos) {
            m_settings.ftMode.store(atoi(request.c_str() + pos + 7) != 0);
        }
        if ((pos = request.find("modespeed=")) != std::string::npos) {
            m_settings.modeSpeed.store(atoi(request.c_str() + pos + 10));
        }
        if ((pos = request.find("animspeed=")) != std::string::npos) {
            m_settings.animSpeed.store(atoi(request.c_str() + pos + 10));
        }

        response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nSettings updated!";
    }
    else if (request.find("GET /reload") != std::string::npos) {
        if (m_reloadCallback) {
            m_reloadCallback();
        }
        response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nLua effects reloaded!";
    }
    else if (request.find("GET /poweroff") != std::string::npos) {
        response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nShutting down...";
        write(clientSocket, response.c_str(), response.length());
        close(clientSocket);
        system("sudo poweroff");
        return;
    }
    else if (request.find("GET /status") != std::string::npos) {
        std::ostringstream json;
        json << "{\"effect\":" << m_settings.currentEffect.load()
             << ",\"brightness\":" << m_settings.brightness.load()
             << ",\"sensitivity\":" << m_settings.sensitivity.load()
             << ",\"threshold\":" << m_settings.noiseThreshold.load()
             << ",\"duration\":" << m_settings.effectDuration.load()
             << ",\"modespeed\":" << m_settings.modeSpeed.load()
             << ",\"animspeed\":" << m_settings.animSpeed.load()
             << ",\"autoloop\":" << (m_settings.autoLoop.load() ? "true" : "false")
             << ",\"ftmode\":" << (m_settings.ftMode.load() ? "true" : "false") << "}";
        response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + json.str();
    }
    else {
        std::string html = generateHTML();
        response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + html;
    }

    write(clientSocket, response.c_str(), response.length());
    close(clientSocket);
}

std::string WebServer::generateHTML() {
    std::ostringstream html;
    html << R"HTMLPAGE(
<!DOCTYPE html>
<html>
<head>
    <title>LED Matrix Control</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; max-width: 600px; margin: 20px auto; padding: 10px; background: #1a1a2e; color: #eee; }
        h1 { color: #00d4ff; text-align: center; }
        .control { margin: 20px 0; padding: 15px; background: #16213e; border-radius: 10px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        input[type=range] { width: 100%; margin: 10px 0; }
        select { width: 100%; padding: 10px; font-size: 16px; background: #0f3460; color: #fff; border: none; border-radius: 5px; }
        .value { text-align: right; color: #00d4ff; font-size: 18px; }
        button { width: 100%; padding: 15px; font-size: 18px; background: #e94560; color: white; border: none; border-radius: 5px; cursor: pointer; margin-top: 10px; }
        button:hover { background: #ff6b6b; }
        button.secondary { background: #0f3460; }
        button.secondary:hover { background: #16213e; }
        button.danger { background: #8b0000; }
        button.danger:hover { background: #b22222; }
        .status { text-align: center; padding: 10px; background: #0f3460; border-radius: 5px; margin-top: 10px; }
        .mode-switch { display: flex; gap: 10px; }
        .mode-btn { flex: 1; padding: 15px; font-size: 16px; border: none; border-radius: 5px; cursor: pointer; transition: all 0.3s; }
        .mode-btn.active { background: #00d4ff; color: #1a1a2e; font-weight: bold; }
        .mode-btn:not(.active) { background: #0f3460; color: #fff; }
        .mode-btn:hover:not(.active) { background: #1a3a5c; }
        .ft-info { background: #0f3460; padding: 10px; border-radius: 5px; margin-top: 10px; font-size: 14px; text-align: center; }
        .audio-controls { }
        .audio-controls.hidden { display: none; }
    </style>
</head>
<body>
    <h1>LED Matrix Control</h1>

    <div class="control">
        <label>Display Mode</label>
        <div class="mode-switch">
            <button class="mode-btn active" id="btnAudio" onclick="setMode(0)">Audio Visualizer</button>
            <button class="mode-btn" id="btnFT" onclick="setMode(1)">Flaschen-Taschen</button>
        </div>
        <div class="ft-info" id="ftInfo" style="display:none;">
            UDP Server on port 1337<br>
            Send PPM (P6) images to display
        </div>
    </div>

    <div class="control">
        <label>Brightness</label>
        <input type="range" id="brightness" min="10" max="255" value="180" oninput="update()">
        <div class="value" id="brightnessVal">180</div>
    </div>

    <div class="audio-controls" id="audioControls">
    <div class="control">
        <label>Effect</label>
        <select id="effect" onchange="update()">
            <option value="-1">Auto (cycle)</option>
)HTMLPAGE";

    // Add effect options
    for (size_t i = 0; i < m_effectNames.size(); i++) {
        html << "            <option value=\"" << i << "\">" << m_effectNames[i] << "</option>\n";
    }

    html << R"HTMLPAGE(
        </select>
    </div>

    <div class="control">
        <label>Sensitivity</label>
        <input type="range" id="sensitivity" min="10" max="500" value="100" oninput="update()">
        <div class="value" id="sensitivityVal">100%</div>
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
        <label>Animation Speed</label>
        <input type="range" id="animspeed" min="10" max="200" value="100" oninput="update()">
        <div class="value" id="animspeedVal">100%</div>
    </div>

    <div class="control">
        <label style="display: inline;">Auto Loop Effects</label>
        <input type="checkbox" id="autoloop" checked onchange="update()" style="width: 24px; height: 24px; margin-left: 10px; vertical-align: middle;">
        <span id="autoloopStatus" style="margin-left: 10px; color: #00d4ff;">ON</span>
    </div>

    <button class="secondary" onclick="reloadLua()">Reload Lua Effects</button>
    </div>

    <button class="danger" onclick="powerOff()">Power Off Panel</button>

    <div class="status" id="status">Ready</div>

    <script>
        var currentFtMode = 0;

        function setMode(mode) {
            currentFtMode = mode;
            document.getElementById("btnAudio").className = mode == 0 ? "mode-btn active" : "mode-btn";
            document.getElementById("btnFT").className = mode == 1 ? "mode-btn active" : "mode-btn";
            document.getElementById("ftInfo").style.display = mode == 1 ? "block" : "none";
            document.getElementById("audioControls").className = mode == 1 ? "audio-controls hidden" : "audio-controls";
            update();
        }

        function update() {
            var effect = document.getElementById("effect").value;
            var brightness = document.getElementById("brightness").value;
            var sensitivity = document.getElementById("sensitivity").value;
            var threshold = document.getElementById("threshold").value;
            var duration = document.getElementById("duration").value;
            var modespeed = document.getElementById("modespeed").value;
            var animspeed = document.getElementById("animspeed").value;
            var autoloop = document.getElementById("autoloop").checked ? 1 : 0;

            document.getElementById("brightnessVal").textContent = brightness;
            document.getElementById("sensitivityVal").textContent = sensitivity + "%";
            document.getElementById("thresholdVal").textContent = (threshold/100).toFixed(2);
            document.getElementById("durationVal").textContent = duration + "s";
            document.getElementById("modespeedVal").textContent = modespeed + "s";
            document.getElementById("animspeedVal").textContent = animspeed + "%";
            document.getElementById("autoloopStatus").textContent = autoloop ? "ON" : "OFF";

            fetch("/set?effect=" + effect + "&brightness=" + brightness +
                  "&sensitivity=" + sensitivity + "&threshold=" + threshold +
                  "&duration=" + duration + "&modespeed=" + modespeed + "&animspeed=" + animspeed +
                  "&autoloop=" + autoloop + "&ftmode=" + currentFtMode)
                .then(r => r.text())
                .then(t => document.getElementById("status").textContent = t)
                .catch(e => document.getElementById("status").textContent = "Error: " + e);
        }

        function reloadLua() {
            fetch("/reload")
                .then(r => r.text())
                .then(t => {
                    document.getElementById("status").textContent = t;
                    setTimeout(() => location.reload(), 1000);
                })
                .catch(e => document.getElementById("status").textContent = "Error: " + e);
        }

        function powerOff() {
            if (confirm("Are you sure you want to shut down the LED panel?")) {
                document.getElementById("status").textContent = "Shutting down...";
                fetch("/poweroff")
                    .then(r => r.text())
                    .then(t => document.getElementById("status").textContent = t)
                    .catch(e => document.getElementById("status").textContent = "Shutdown initiated");
            }
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
                document.getElementById("animspeed").value = data.animspeed;
                document.getElementById("autoloop").checked = data.autoloop;
                document.getElementById("brightnessVal").textContent = data.brightness;
                document.getElementById("sensitivityVal").textContent = data.sensitivity + "%";
                document.getElementById("thresholdVal").textContent = data.threshold.toFixed(2);
                document.getElementById("durationVal").textContent = data.duration + "s";
                document.getElementById("modespeedVal").textContent = data.modespeed + "s";
                document.getElementById("animspeedVal").textContent = data.animspeed + "%";
                document.getElementById("autoloopStatus").textContent = data.autoloop ? "ON" : "OFF";
                setMode(data.ftmode ? 1 : 0);
            });
    </script>
</body>
</html>
)HTMLPAGE";

    return html.str();
}
