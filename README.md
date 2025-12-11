# Audio LED Visualizer for Raspberry Pi

Audio-reactive LED matrix visualizer with plugin architecture supporting C++ and Lua effects, web control interface, Flaschen-Taschen UDP server, and cross-compilation support.

## Features

- **13+ Visual Effects** - Volume bars, spectrum analyzer, plasma, fire, rain, matrix, starfield, and more
- **Plugin System** - Write effects in C++ (fast) or Lua (hot-reloadable)
- **Flaschen-Taschen Mode** - UDP PPM receiver on port 1337 for external image streaming
- **Web Interface** - Control brightness, sensitivity, effects, animation speed from any browser
- **Cross-Compilation** - Build on Windows/Linux for Raspberry Pi (32-bit and 64-bit)

## Hardware

- Raspberry Pi Zero / Zero 2 / Pi 3 / Pi 4
- 128x64 RGB LED Matrix (2x 64x64 panels chained)
- Adafruit RGB Matrix HAT/Bonnet (PWM)
- USB Audio Capture Device

## Quick Start

### Cross-Compile on Windows (Recommended)

The fastest workflow - compile on your PC, deploy to Pi.

**Prerequisites (WSL2):**
```bash
# In WSL Ubuntu
sudo apt update
sudo apt install -y build-essential cmake

# For Pi Zero 2 W / Pi 3/4 with 64-bit OS (aarch64):
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# For Pi Zero / older Pi with 32-bit OS (armhf):
sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

**Build for 64-bit Pi (Pi Zero 2 W, Pi 3/4 with 64-bit OS):**
```bash
# Clone rpi-rgb-led-matrix next to this project
cd /mnt/d/Developer/C++
git clone https://github.com/hzeller/rpi-rgb-led-matrix.git

# Cross-compile the LED matrix library for aarch64
cd rpi-rgb-led-matrix/lib
CXX=aarch64-linux-gnu-g++ CC=aarch64-linux-gnu-gcc make

# Build audio_led
cd ../raspi
mkdir build-arm && cd build-arm
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-toolchain.cmake ..
make -j$(nproc)
```

**Build for 32-bit Pi (Pi Zero, older systems):**
```bash
cd rpi-rgb-led-matrix/lib
CXX=arm-linux-gnueabihf-g++ CC=arm-linux-gnueabihf-gcc make

cd ../raspi
mkdir build-arm && cd build-arm
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-toolchain.cmake ..
make -j$(nproc)
```

**Deploy:**
```bash
# Copy binary
scp build-arm/audio_led pi@raspberrypi:~/

# Copy Lua scripts
scp -r effects/scripts pi@raspberrypi:~/effects/
```

### Build on Raspberry Pi

```bash
# Install dependencies
sudo apt update
sudo apt install -y build-essential cmake libasound2-dev liblua5.3-dev

# Clone LED matrix library
cd ~
git clone https://github.com/hzeller/rpi-rgb-led-matrix.git
cd rpi-rgb-led-matrix && make

# Build
cd ~/raspi
mkdir build && cd build
cmake .. && make -j2
```

## Running

```bash
# Basic usage (looks for scripts in ./effects/scripts)
sudo ./audio_led

# Specify custom scripts directory
sudo ./audio_led --scripts /path/to/scripts
```

**Web interface:** `http://<raspberry-pi-ip>:8080`

**Flaschen-Taschen:** UDP port `1337` - send PPM (P6) images to display external content

## ALSA Audio Configuration

The audio device must be accessible when running as root.

**1. Find your audio device:**
```bash
arecord -l
```

**2. Create /etc/asound.conf:**
```
pcm.!default {
    type plug
    slave.pcm "hw:0,0"
}

ctl.!default {
    type hw
    card 0
}
```

**3. Set permissions (add to /etc/rc.local):**
```bash
chmod 666 /dev/snd/*
```

## Effects

### Built-in C++ Effects

| # | Effect | Description |
|---|--------|-------------|
| 1 | Volume Bars | Multi-mode volume visualization with shapes |
| 2 | Beat Pulse | Pulsing circle on beat detection |
| 3 | Spectrum | 8-band FFT spectrum analyzer with rainbow colors |
| 4 | Plasma | Animated plasma effect modulated by audio |
| 5 | Fire | Rising flames with heat from audio volume |
| 6 | Rain | Falling raindrops, speed based on volume |
| 7 | Matrix | Matrix-style falling green characters |
| 8 | Starfield | 3D starfield flying through space |
| 9 | VU Meter | Classic stereo VU meter (left=red, right=green) |
| 10 | Waveform | Audio-reactive waveform with color cycling |
| 11 | Color Pulse | Full screen color pulsing with audio |
| 12 | Color Wipe | Color wipe transitions in 4 directions |
| 13 | Spectrum 3D | 3D waterfall spectrum analyzer |

### Included Lua Effects

| Effect | Description |
|--------|-------------|
| Bass Pulse | Pulsing circle based on bass level |
| Rainbow Bars | Spectrum analyzer with animated rainbow |
| Pacman | Pacman and ghosts react to audio |

Custom Lua effects can be added to `effects/scripts/`

## Web Interface Controls

### Display Mode
- **Audio Visualizer** - Audio-reactive effects mode
- **Flaschen-Taschen** - UDP PPM receiver mode (port 1337)

### Audio Controls (visible in Audio Visualizer mode)
- **Effect Selection** - Choose specific effect or auto-cycle
- **Brightness** - LED brightness (10-255)
- **Sensitivity** - Audio sensitivity multiplier (10-500%)
- **Noise Threshold** - Filter out background noise (0-100%)
- **Effect Duration** - Seconds per effect in auto mode (2-60s)
- **Mode Change Speed** - Seconds between Volume Bars sub-mode changes (1-30s)
- **Animation Speed** - Animation speed multiplier (10-200%)
- **Auto Loop** - Toggle automatic effect cycling
- **Reload Lua Effects** - Hot-reload Lua scripts without restart

## Project Structure

```
raspi/
├── src/                    # Core source files
│   ├── main.cpp            # Application entry + FT server
│   ├── effect_manager.*    # Effect loading/management
│   ├── lua_effect.*        # Lua script wrapper
│   ├── audio_capture.*     # ALSA + FFT
│   └── web_server.*        # HTTP control + web UI
├── effects/
│   ├── builtin/            # C++ effects (compiled)
│   └── scripts/            # Lua effects (hot-reload)
├── libs/
│   ├── lua/                # Embedded Lua 5.3 source
│   ├── alsa/               # ALSA headers
│   ├── arm/                # 32-bit ARM libs (armhf)
│   └── aarch64/            # 64-bit ARM libs (Pi Zero 2 W)
├── cmake/
│   ├── arm-toolchain.cmake      # 32-bit cross-compile
│   └── aarch64-toolchain.cmake  # 64-bit cross-compile
├── kissfft/                # FFT library
└── docs/                   # Documentation
```

## Writing Custom Effects

### Lua Effect (Recommended for prototyping)

Create `effects/scripts/my_effect.lua`:

```lua
effect_name = "My Effect"
effect_description = "Custom audio visualization"

function init(width, height)
    -- Called once on load
end

function update(audio, settings, time)
    clear()

    -- audio.volume, audio.beat, audio.bass, audio.spectrum[1-8]
    local radius = 10 + audio.beat * 30
    fillCircle(WIDTH/2, HEIGHT/2, radius, 1, 0, 0.5)
end
```

No recompilation needed - save and click "Reload Lua" in web UI.

### C++ Effect (For performance-critical effects)

See `docs/DEVELOPMENT.md` for full C++ effect guide.

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "Cannot get card index" | Create `/etc/asound.conf` as described above |
| GPIO access denied | Stop ft-server: `sudo systemctl stop ftserver` |
| No audio input | Check `arecord -l` and update asound.conf card number |
| Lua effects not in dropdown | Copy `effects/scripts/` to Pi, or use `--scripts` flag |
| Lua effect syntax error | Check terminal output for Lua error messages |
| Wrong architecture | Use `file audio_led` to check binary type matches your Pi |

## Documentation

- [Quick Start Guide](docs/QUICK_START.md)
- [Development Guide](docs/DEVELOPMENT.md)

## License

MIT License - See LICENSE file
