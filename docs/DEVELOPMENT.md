# Audio LED Visualizer - Development Guide

## Project Overview

This is a modular audio-reactive LED visualizer for Raspberry Pi with a hybrid plugin architecture supporting both high-performance C++ effects and hot-reloadable Lua scripts.

## Architecture

```
audio_led/
├── src/                      # Core source files
│   ├── main.cpp              # Application entry point
│   ├── effect.h              # Base effect interface
│   ├── effect_manager.h/cpp  # Effect loading and management
│   ├── audio_capture.h/cpp   # ALSA audio input + FFT
│   ├── web_server.h/cpp      # HTTP control interface
│   └── lua_effect.h/cpp      # Lua script wrapper
│
├── effects/
│   ├── builtin/              # C++ effects (compiled, fast)
│   │   ├── effect_volume.h
│   │   ├── effect_spectrum.h
│   │   ├── effect_plasma.h
│   │   └── effect_fire.h
│   │
│   └── scripts/              # Lua effects (hot-reloadable)
│       ├── bass_pulse.lua
│       └── rainbow_bars.lua
│
├── kissfft/                  # FFT library
├── libs/                     # Prebuilt dependencies
├── presets/                  # JSON configuration presets
├── docs/                     # Documentation
│
├── CMakeLists.txt            # CMake build configuration
├── Makefile                  # Legacy Makefile (v1.0)
└── audio_led.cpp             # Legacy monolithic source (v1.0)
```

## Development Workflow

### Option 1: Cross-Compile on Windows (Recommended)

Cross-compiling from Windows is the fastest development workflow since the Pi Zero is slow for compilation.

#### Prerequisites

1. **Install WSL2 (Windows Subsystem for Linux)**
   ```powershell
   wsl --install -d Ubuntu-22.04
   ```

2. **Install Cross-Compiler in WSL**
   ```bash
   sudo apt update
   sudo apt install -y build-essential cmake git
   sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
   sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
   ```

3. **Install Target Libraries**
   ```bash
   # For ARM64 (Pi Zero 2, Pi 3, Pi 4)
   sudo dpkg --add-architecture arm64

   # For ARMhf (Pi Zero, Pi 1, Pi 2)
   sudo dpkg --add-architecture armhf
   ```

#### Cross-Compile Steps

1. **Clone the rpi-rgb-led-matrix library**
   ```bash
   cd /mnt/d/Developer/C++
   git clone https://github.com/hzeller/rpi-rgb-led-matrix.git
   cd rpi-rgb-led-matrix

   # Cross-compile for Pi
   make HARDWARE_DESC=adafruit-hat-pwm \
        CXX=arm-linux-gnueabihf-g++ \
        CC=arm-linux-gnueabihf-gcc
   ```

2. **Build audio_led**
   ```bash
   cd /mnt/d/Developer/C++/raspi
   mkdir build-arm && cd build-arm

   cmake .. \
     -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-toolchain.cmake \
     -DCMAKE_BUILD_TYPE=Release

   make -j$(nproc)
   ```

3. **Deploy to Pi**
   ```bash
   scp audio_led pi@raspberrypi:/home/pi/
   scp -r effects/scripts pi@raspberrypi:/home/pi/effects/
   ```

### Option 2: Compile Directly on Raspberry Pi

Slower but simpler - no cross-compile setup needed.

```bash
# On the Raspberry Pi
cd ~/audio_led

# Install dependencies
sudo apt update
sudo apt install -y build-essential cmake
sudo apt install -y libasound2-dev
sudo apt install -y liblua5.3-dev   # or libluajit-5.1-dev

# Clone LED matrix library
cd ~
git clone https://github.com/hzeller/rpi-rgb-led-matrix.git
cd rpi-rgb-led-matrix
make

# Build audio_led
cd ~/audio_led
mkdir build && cd build
cmake ..
make -j2   # Pi Zero has limited RAM, use -j2
```

### Option 3: Docker Cross-Compile

Create a reproducible build environment.

```dockerfile
# Dockerfile.cross
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
    libasound2-dev:armhf liblua5.3-dev:armhf

WORKDIR /build
```

```bash
# Build and run
docker build -t audio_led-cross -f Dockerfile.cross .
docker run -v $(pwd):/build audio_led-cross make
```

## Creating New Effects

### C++ Builtin Effect

Create `effects/builtin/effect_myeffect.h`:

```cpp
#pragma once
#include "../../src/effect.h"
#include "led-matrix.h"

class EffectMyEffect : public Effect {
public:
    std::string getName() const override { return "My Effect"; }
    std::string getDescription() const override {
        return "Description of my effect";
    }

    void init(int width, int height) override {
        Effect::init(width, height);
        // Initialize state here
    }

    void reset() override {
        // Reset state when effect is switched away
    }

    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override {

        // Access audio data
        float volume = audio.volume;
        float beat = audio.beat;
        float bass = audio.bass;
        float spectrum[8] = audio.spectrum;  // 8-band FFT

        // Access settings
        int brightness = settings.brightness;
        float threshold = settings.noiseThreshold;

        // Draw pixels
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                canvas->SetPixel(x, y, r, g, b);
            }
        }
    }

private:
    // Effect state variables
};
```

Register in `src/main.cpp`:
```cpp
#include "effect_myeffect.h"
// ...
effectManager.registerEffect(std::make_unique<EffectMyEffect>());
```

### Lua Scripted Effect

Create `effects/scripts/my_effect.lua`:

```lua
-- Effect metadata
effect_name = "My Lua Effect"
effect_description = "A custom Lua effect"

-- Local state (persists between frames)
local my_state = 0

-- Called once when effect loads
function init(width, height)
    my_state = 0
end

-- Called when effect is reset
function reset()
    my_state = 0
end

-- Called every frame (~60 FPS)
-- audio: { volume, beat, bass, mid, treble, spectrum[1-8] }
-- settings: { brightness, sensitivity, noiseThreshold }
-- time: elapsed seconds
function update(audio, settings, time)
    -- Clear screen
    clear()

    -- Use audio data
    local vol = audio.volume
    local beat = audio.beat
    local bass = audio.bass

    -- Check noise threshold
    if vol < settings.noiseThreshold then
        vol = 0
    end

    -- Draw with available functions:
    -- setPixel(x, y, r, g, b)      -- RGB 0-1
    -- setPixelHSV(x, y, h, s, v)   -- HSV, h=0-360
    -- clear(r, g, b)               -- Clear to color
    -- drawLine(x1, y1, x2, y2, r, g, b)
    -- drawRect(x, y, w, h, r, g, b)
    -- fillRect(x, y, w, h, r, g, b)
    -- drawCircle(cx, cy, radius, r, g, b)
    -- fillCircle(cx, cy, radius, r, g, b)

    -- Global constants: WIDTH, HEIGHT

    -- Example: pulsing circle
    local radius = 10 + beat * 20
    fillCircle(WIDTH/2, HEIGHT/2, radius, 1, 0, 0.5)
end
```

No recompilation needed - just save the file and click "Reload Lua Effects" in web UI.

## Lua API Reference

### Drawing Functions

| Function | Parameters | Description |
|----------|------------|-------------|
| `setPixel` | `x, y, r, g, b` | Set pixel RGB (values 0-1) |
| `setPixelHSV` | `x, y, h, s, v` | Set pixel HSV (h: 0-360, s/v: 0-1) |
| `clear` | `[r, g, b]` | Clear to black or specified color |
| `drawLine` | `x1, y1, x2, y2, r, g, b` | Draw line |
| `fillRect` | `x, y, w, h, r, g, b` | Filled rectangle |
| `drawRect` | `x, y, w, h, r, g, b` | Rectangle outline |
| `fillCircle` | `cx, cy, radius, r, g, b` | Filled circle |
| `drawCircle` | `cx, cy, radius, r, g, b` | Circle outline |

### Audio Data (passed to update)

| Field | Type | Description |
|-------|------|-------------|
| `volume` | float | Overall volume level |
| `beat` | float | Beat detection (0-1) |
| `bass` | float | Low frequency energy |
| `mid` | float | Mid frequency energy |
| `treble` | float | High frequency energy |
| `spectrum` | table[1-8] | 8-band FFT spectrum |

### Settings (passed to update)

| Field | Type | Description |
|-------|------|-------------|
| `brightness` | int | LED brightness (0-255) |
| `sensitivity` | float | Audio sensitivity multiplier |
| `noiseThreshold` | float | Volume threshold for effects |

### Constants

| Name | Value | Description |
|------|-------|-------------|
| `WIDTH` | 128 | Display width in pixels |
| `HEIGHT` | 64 | Display height in pixels |

## Performance Tips

### C++ Effects
- Pre-calculate lookup tables (sin/cos tables)
- Avoid allocations in update loop
- Use integer math where possible
- Consider NEON SIMD for Pi 2/3/4

### Lua Effects
- Use LuaJIT instead of standard Lua (10-50x faster)
- Avoid creating tables in the update loop
- Use local variables (faster than globals)
- Pre-compute values in init()
- Use builtin drawing functions (C++ implemented)

### General
- Target 60 FPS (16ms per frame)
- Profile with `time` parameter logging
- Simple effects work better on Pi Zero

## Debugging

### Check Audio
```bash
# List audio devices
arecord -l

# Test recording
arecord -D plughw:0,0 -f S16_LE -r 44100 -d 5 test.wav
aplay test.wav
```

### Check Web Interface
```bash
# From another machine
curl http://raspberrypi:8080/status
```

### View Logs
```bash
# Run in foreground for debug output
sudo ./audio_led 2>&1 | tee debug.log
```

## Next Steps

1. [ ] Add CMake toolchain file for cross-compilation
2. [ ] Port remaining effects to new architecture
3. [ ] Add preset save/load functionality
4. [ ] Implement Lua effect hot-reload without restart
5. [ ] Add effect parameters configurable via web UI
6. [ ] Add audio device selection in web UI
7. [ ] Add BPM detection for beat-sync effects
8. [ ] Create effect gallery/preview images
