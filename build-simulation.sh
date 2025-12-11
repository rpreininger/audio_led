#!/bin/bash
# Build script for LED matrix simulation on WSL/Linux

set -e

echo "==================================="
echo "LED Matrix Simulation Build Script"
echo "==================================="

# Check for required dependencies
echo "Checking dependencies..."
MISSING_DEPS=""

if ! pkg-config --exists sdl2; then
    MISSING_DEPS="$MISSING_DEPS libsdl2-dev"
fi

if ! pkg-config --exists alsa; then
    MISSING_DEPS="$MISSING_DEPS libasound2-dev"
fi

if [ ! -z "$MISSING_DEPS" ]; then
    echo "Missing dependencies:$MISSING_DEPS"
    echo ""
    echo "Install with:"
    echo "  sudo apt-get update"
    echo "  sudo apt-get install$MISSING_DEPS"
    exit 1
fi

# Build rpi-rgb-led-matrix with simulation support
echo ""
echo "Building rpi-rgb-led-matrix library with simulation support..."
cd ../rpi-rgb-led-matrix

# Check if we need to rebuild
if [ ! -f lib/librgbmatrix.a ] || grep -q "EMULATE_GPIO" lib/Makefile 2>/dev/null; then
    echo "Building with HARDWARE_DESC=simulator..."
    make clean
    HARDWARE_DESC=simulator make -j$(nproc)
else
    echo "Library already built, skipping..."
fi

# Build the audio_led project
echo ""
echo "Building audio_led with simulation mode..."
cd ../raspi

# Create build directory
mkdir -p build-simulation
cd build-simulation

# Configure with CMake
cmake .. -DENABLE_SIMULATION=ON

# Build
make -j$(nproc)

echo ""
echo "==================================="
echo "Build complete!"
echo "==================================="
echo ""
echo "To run the simulation:"
echo "  cd build-simulation"
echo "  ./audio_led"
echo ""
echo "The LED matrix will be displayed in an SDL2 window."
echo "Web interface will be available at http://localhost:8080"
echo ""
