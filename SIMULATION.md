# LED Matrix Simulation Guide

This guide explains how to test the LED matrix project on WSL or Linux without Raspberry Pi hardware using SDL2 simulation.

## Prerequisites

Install required dependencies on WSL/Ubuntu:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config
sudo apt-get install libsdl2-dev libasound2-dev
```

## Quick Start

Run the automated build script:

```bash
cd /mnt/d/Developer/C++/raspi
./build-simulation.sh
```

This will:
1. Check for required dependencies
2. Build the rpi-rgb-led-matrix library with simulator support
3. Build audio_led with simulation mode enabled

## Manual Build

If you prefer to build manually:

### 1. Build rpi-rgb-led-matrix with simulation

```bash
cd ../rpi-rgb-led-matrix
make clean
HARDWARE_DESC=simulator make -j$(nproc)
```

### 2. Build audio_led with simulation

```bash
cd ../raspi
mkdir -p build-simulation
cd build-simulation
cmake .. -DENABLE_SIMULATION=ON
make -j$(nproc)
```

## Running the Simulation

```bash
cd build-simulation
./audio_led
```

An SDL2 window will appear showing the 128x64 LED matrix simulation.

## Features in Simulation Mode

- **Visual LED Panel**: SDL2 window displays the LED matrix
- **Audio Input**: Still captures audio from ALSA (e.g., PulseAudio monitor)
- **Web Interface**: Available at http://localhost:8080
- **All Effects**: Builtin C++ effects and Lua scripts work identically
- **Hot Reload**: Lua effects can be reloaded via web interface

## Audio Setup for WSL

WSL can capture audio from Windows via PulseAudio. To test audio visualization:

### Option 1: Use a test tone
```bash
speaker-test -t sine -f 440
```

### Option 2: Configure PulseAudio loopback
```bash
pactl load-module module-loopback
```

### Option 3: Record from Windows audio
Configure PulseAudio to use the Windows audio sink as a source.

## Troubleshooting

### SDL2 Window doesn't appear

Make sure you have X11 forwarding enabled:
```bash
export DISPLAY=:0
```

For WSL2, install an X server like VcXsrv or WSLg should work automatically.

### No audio capture

Check ALSA devices:
```bash
arecord -L
```

The project defaults to capturing from `default`. You can modify `src/audio_capture.cpp` to use a specific device.

### Library not found errors

Make sure the rpi-rgb-led-matrix library is built first:
```bash
ls -la ../rpi-rgb-led-matrix/lib/librgbmatrix.a
```

## Differences from Hardware

The simulation mode has a few differences:

1. **No GPIO**: Hardware GPIO pins are not accessed
2. **Timing**: Frame timing may differ from hardware PWM
3. **Performance**: Typically faster on desktop hardware
4. **Display**: SDL2 window instead of physical LEDs

## Development Workflow

1. Write/modify effects in `effects/scripts/`
2. Build with simulation: `./build-simulation.sh`
3. Run and test: `cd build-simulation && ./audio_led`
4. Make changes and use hot reload (Ctrl+R in web interface)
5. When satisfied, cross-compile for Raspberry Pi

## Next Steps

- Modify effects in `effects/builtin/` (C++) or `effects/scripts/` (Lua)
- Test with different audio sources
- Adjust parameters via web interface at http://localhost:8080
- When ready, build for Pi using `./build-arm.sh`
