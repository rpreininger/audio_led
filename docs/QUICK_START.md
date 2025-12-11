# Audio LED Visualizer - Quick Start Guide

## Step 1: Setup WSL2 on Windows

Open PowerShell as Administrator:

```powershell
wsl --install -d Ubuntu-22.04
```

Restart your computer when prompted. After restart, Ubuntu will open and ask you to create a username and password.

---

## Step 2: Setup Cross-Compile Environment in WSL2

Open Ubuntu (WSL2) and run:

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install build tools
sudo apt install -y build-essential cmake git pkg-config

# Install cross-compilers
sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# Install Lua development files
sudo apt install -y liblua5.3-dev
```

---

## Step 3: Clone and Build LED Matrix Library

```bash
# Navigate to your project folder (Windows D: drive is /mnt/d in WSL)
cd /mnt/d/Developer/C++

# Clone the LED matrix library
git clone https://github.com/hzeller/rpi-rgb-led-matrix.git
cd rpi-rgb-led-matrix

# Build for native Linux first (to get headers)
make
```

---

## Step 4: Build Audio LED Project

```bash
# Go to your project
cd /mnt/d/Developer/C++/raspi

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
make -j$(nproc)
```

**Note:** This builds a native Linux version for testing. Cross-compilation requires additional setup (see DEVELOPMENT.md).

---

## Step 5: Deploy to Raspberry Pi

### Option A: Copy files manually

```bash
# From WSL2
scp /mnt/d/Developer/C++/raspi/build/audio_led pi@raspberrypi:/home/pi/
scp -r /mnt/d/Developer/C++/raspi/effects/scripts pi@raspberrypi:/home/pi/effects/
```

### Option B: Build directly on Pi

```bash
# SSH to Pi
ssh pi@raspberrypi

# Install dependencies
sudo apt update
sudo apt install -y build-essential cmake git
sudo apt install -y libasound2-dev liblua5.3-dev

# Clone LED matrix library
cd ~
git clone https://github.com/hzeller/rpi-rgb-led-matrix.git
cd rpi-rgb-led-matrix
make

# Clone your project
git clone https://github.com/rpreininger/audio_led.git
cd audio_led
git checkout feature/plugin-system

# Build
mkdir build && cd build
cmake ..
make -j2
```

---

## Step 6: Configure Audio on Pi

```bash
# SSH to Pi
ssh pi@raspberrypi

# Check audio device
arecord -l

# Create ALSA config
sudo nano /etc/asound.conf
```

Add this content:
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

Set permissions:
```bash
sudo chmod 644 /etc/asound.conf
sudo chmod 666 /dev/snd/*
```

Add to `/etc/rc.local` (before `exit 0`) for persistence:
```bash
chmod 666 /dev/snd/*
```

---

## Step 7: Run

```bash
# On the Pi
cd ~/audio_led/build
sudo ./audio_led
```

Web interface: `http://raspberrypi:8080`

---

## Step 8: Setup Autostart (Optional)

```bash
# Copy service file
sudo cp ~/audio_led/audio_led.service /etc/systemd/system/

# Enable and start
sudo systemctl enable audio_led
sudo systemctl start audio_led

# Check status
sudo systemctl status audio_led
```

---

## Troubleshooting

### "Cannot open audio device"
```bash
sudo chmod 666 /dev/snd/*
```

### "Failed to create LED matrix"
```bash
# Stop conflicting services
sudo systemctl stop ftserver.service
sudo pkill ft-server
```

### Check logs
```bash
sudo journalctl -u audio_led -f
```
