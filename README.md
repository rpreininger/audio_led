# Audio LED Visualizer for Raspberry Pi

Audio-reactive LED matrix visualizer with plugin architecture supporting C++ and Lua effects, web control interface, and cross-compilation support.

## Features

- **12+ Visual Effects** - Volume bars, spectrum analyzer, plasma, fire, rain, matrix, and more
- **Plugin System** - Write effects in C++ (fast) or Lua (hot-reloadable)
- **Web Interface** - Control brightness, sensitivity, effects from any browser
- **Cross-Compilation** - Build on Windows/Linux for Raspberry Pi

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
sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
```

**Build:**
```bash
# Clone rpi-rgb-led-matrix next to this project
cd /mnt/d/Developer/C++
git clone https://github.com/hzeller/rpi-rgb-led-matrix.git

# Cross-compile the LED matrix library
cd rpi-rgb-led-matrix/lib
CXX=arm-linux-gnueabihf-g++ CC=arm-linux-gnueabihf-gcc make

# Build audio_led
cd ../raspi
mkdir build-arm && cd build-arm
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-toolchain.cmake ..
make -j4
```

**Deploy:**
```bash
scp build-arm/audio_led pi@raspberrypi:~/
scp libs/arm/libasound.so.2 pi@raspberrypi:~/
ssh pi@raspberrypi 'sudo mv ~/libasound.so.2 /usr/lib/arm-linux-gnueabihf/'
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
sudo ./audio_led
```

Web interface: `http://<raspberry-pi-ip>:8080`

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

| # | Effect | Description |
|---|--------|-------------|
| 1 | Volume Bars | Green/cyan gradient bars responding to volume |
| 2 | Beat Pulse | Pulsing circle on beat detection |
| 3 | Spectrum | 8-band FFT spectrum analyzer with rainbow colors |
| 4 | Plasma | Animated plasma effect modulated by audio |
| 5 | Fire | Rising flames with heat from audio volume |
| 6 | Rain | Falling raindrops, speed based on volume |
| 7 | Matrix | Matrix-style falling characters |
| 8 | Starfield | 3D starfield flying through space |
| 9 | VU Meter | Classic VU meter (left=red, right=green) |
| 10 | Waveform | Scrolling audio waveform display |
| 11 | Color Pulse | Full screen color pulsing with audio |
| 12 | Color Wipe | Color wipe transitions in 4 directions |

Plus any custom Lua effects in `effects/scripts/`

## Web Interface Controls

- **Effect Selection** - Choose specific effect or auto-cycle
- **Brightness** - LED brightness (10-255)
- **Sensitivity** - Audio sensitivity multiplier
- **Noise Threshold** - Filter out background noise
- **Effect Duration** - Seconds per effect in auto mode
- **Auto Loop** - Toggle automatic effect cycling
- **Reload Lua** - Hot-reload Lua effects without restart

## Project Structure

```
raspi/
├── src/                    # Core source files
│   ├── main.cpp            # Application entry
│   ├── effect_manager.*    # Effect loading/management
│   ├── lua_effect.*        # Lua script wrapper
│   ├── audio_capture.*     # ALSA + FFT
│   └── web_server.*        # HTTP control
├── effects/
│   ├── builtin/            # C++ effects (compiled)
│   └── scripts/            # Lua effects (hot-reload)
├── libs/
│   ├── lua/                # Embedded Lua 5.3 source
│   ├── alsa/               # ALSA headers
│   └── arm/                # ARM libraries for cross-compile
├── cmake/
│   └── arm-toolchain.cmake # Cross-compilation toolchain
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
| Lua effect not loading | Check for syntax errors in terminal output |

## Documentation

- [Quick Start Guide](docs/QUICK_START.md)
- [Development Guide](docs/DEVELOPMENT.md)

## License

MIT License - See LICENSE file
