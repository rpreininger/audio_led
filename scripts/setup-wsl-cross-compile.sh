#!/bin/bash
# ====================================================================
# Setup script for WSL2 cross-compilation environment
# Run this inside WSL2 Ubuntu
# ====================================================================

set -e

echo "=== Audio LED Cross-Compile Setup for WSL2 ==="
echo ""

# Update package list
echo "Updating package list..."
sudo apt update

# Install build essentials
echo "Installing build tools..."
sudo apt install -y build-essential cmake git pkg-config

# Install cross-compilers
echo "Installing ARM cross-compilers..."
sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Install native development libraries (for testing)
echo "Installing native libraries..."
sudo apt install -y libasound2-dev liblua5.3-dev

# Note about cross-libraries
echo ""
echo "=== IMPORTANT ==="
echo "Cross-compiled libraries (libasound, liblua) need to be obtained"
echo "from the Raspberry Pi itself or from a Pi sysroot."
echo ""
echo "Simplest approach: Compile on Pi first, then cross-compile for speed."
echo ""

# Check if rpi-rgb-led-matrix exists
RGB_MATRIX_PATH="../rpi-rgb-led-matrix"
if [ ! -d "$RGB_MATRIX_PATH" ]; then
    echo "Cloning rpi-rgb-led-matrix library..."
    cd ..
    git clone https://github.com/hzeller/rpi-rgb-led-matrix.git
    cd -
fi

echo ""
echo "=== Setup Complete ==="
echo ""
echo "To build natively (for testing on Linux):"
echo "  mkdir build && cd build"
echo "  cmake .."
echo "  make"
echo ""
echo "To cross-compile for Pi Zero (32-bit):"
echo "  mkdir build-arm && cd build-arm"
echo "  cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-toolchain.cmake .."
echo "  make"
echo ""
echo "To cross-compile for Pi Zero 2/3/4 (64-bit):"
echo "  mkdir build-arm64 && cd build-arm64"
echo "  cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-toolchain.cmake .."
echo "  make"
echo ""
