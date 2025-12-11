#!/bin/bash
# ====================================================================
# Deploy script - copy built files to Raspberry Pi
# Usage: ./deploy-to-pi.sh [pi-hostname] [build-dir]
# ====================================================================

PI_HOST="${1:-raspberrypi}"
BUILD_DIR="${2:-build}"
PI_USER="pi"
PI_PATH="/home/pi/audio_led"

echo "=== Deploying to $PI_USER@$PI_HOST:$PI_PATH ==="

# Check if binary exists
if [ ! -f "$BUILD_DIR/audio_led" ]; then
    echo "Error: $BUILD_DIR/audio_led not found!"
    echo "Build the project first: cd $BUILD_DIR && make"
    exit 1
fi

# Create directory on Pi
echo "Creating directory on Pi..."
ssh "$PI_USER@$PI_HOST" "mkdir -p $PI_PATH/effects/scripts"

# Copy binary
echo "Copying binary..."
scp "$BUILD_DIR/audio_led" "$PI_USER@$PI_HOST:$PI_PATH/"

# Copy Lua scripts
echo "Copying Lua effects..."
scp -r effects/scripts/*.lua "$PI_USER@$PI_HOST:$PI_PATH/effects/scripts/"

# Copy presets if they exist
if [ -d "presets" ]; then
    echo "Copying presets..."
    ssh "$PI_USER@$PI_HOST" "mkdir -p $PI_PATH/presets"
    scp -r presets/*.json "$PI_USER@$PI_HOST:$PI_PATH/presets/" 2>/dev/null || true
fi

echo ""
echo "=== Deployment Complete ==="
echo ""
echo "To run on the Pi:"
echo "  ssh $PI_USER@$PI_HOST"
echo "  cd $PI_PATH"
echo "  sudo ./audio_led"
echo ""
echo "Or to install as service:"
echo "  sudo cp audio_led.service /etc/systemd/system/"
echo "  sudo systemctl enable audio_led"
echo "  sudo systemctl start audio_led"
echo ""
