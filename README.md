# Audio LED Visualizer for Raspberry Pi

Audio-reactive LED matrix visualizer with 12 effects and web control interface.

## Hardware

- Raspberry Pi Zero (or any Raspberry Pi)
- 128x64 RGB LED Matrix (2x 64x64 panels chained)
- Adafruit RGB Matrix HAT/Bonnet (PWM)
- USB Audio Capture Device

## Dependencies

```bash
# Install required packages
sudo apt-get update
sudo apt-get install -y libasound2-dev

# Clone rpi-rgb-led-matrix library (must be in parent directory)
cd ..
git clone https://github.com/hzeller/rpi-rgb-led-matrix.git
cd rpi-rgb-led-matrix
make
```

## Building

```bash
make clean
make
```

## ALSA Audio Configuration (IMPORTANT)

The audio device must be accessible when running as root (sudo). **This is critical** - without this configuration, you will get "Cannot get card index" errors.

### Step 1: Find your audio device

```bash
arecord -l
```

Note the card number (e.g., `card 0: Device`).

### Step 2: Create ALSA config

```bash
sudo nano /etc/asound.conf
```

Add the following content (adjust `hw:0,0` if your card number differs):

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

### Step 3: Set permissions

```bash
sudo chmod 644 /etc/asound.conf
```

### Step 4: Add user to audio group (optional but recommended)

```bash
sudo usermod -a -G audio $USER
```

Reboot after these changes for them to take effect.

## Running

```bash
sudo ./audio_led
```

The LED matrix requires root access for GPIO. The web interface will be available at `http://<raspberry-pi-ip>:8080`

## Stopping ft-server (if running)

If you have flaschen-taschen ft-server running, it will conflict with GPIO access:

```bash
# Stop the service
sudo systemctl stop ftserver.service

# Disable autostart
sudo systemctl disable ftserver.service

# Or if running manually, find and kill
sudo pkill ft-server
```

Also check `/etc/rc.local` and comment out any ft-server autostart lines.

## Effects

1. **Volume Bars** - Green/cyan gradient bar responding to volume
2. **Beat Pulse** - Pulsing circle on beat detection
3. **Spectrum** - 8-band FFT spectrum analyzer with rainbow colors
4. **Plasma** - Animated plasma effect modulated by audio
5. **Fire** - Rising flames with heat from audio volume
6. **Rain** - Falling raindrops, speed based on volume
7. **Matrix** - Matrix-style falling characters
8. **Starfield** - 3D starfield flying through space
9. **VU Meter** - Classic VU meter (left=red, right=green)
10. **Waveform** - Scrolling audio waveform display
11. **Color Pulse** - Full screen color pulsing with audio
12. **Color Wipe** - Color wipe transitions in 4 directions

## Web Interface

Access `http://<raspberry-pi-ip>:8080` to control:

- **Effect Selection** - Choose specific effect or auto-cycle
- **Brightness** - LED brightness (10-255)
- **Sensitivity** - Audio sensitivity multiplier
- **Noise Threshold** - Filter out background noise
- **Effect Duration** - Seconds per effect in auto mode
- **Auto Loop** - Toggle automatic effect cycling

## LED Panel Configuration

The code is configured for:
- `hardware_mapping`: adafruit-hat-pwm
- `rows`: 64
- `cols`: 128
- `chain_length`: 1

Modify in `main()` if your setup differs.

## Troubleshooting

### "Cannot get card index" error
- ALSA config not accessible under sudo
- Create `/etc/asound.conf` as described above

### "Input/output error" on audio read
- PCM stream not started properly
- Code includes automatic recovery for overruns

### Double bars / wrong display
- Check panel configuration matches your hardware
- Verify `rows`, `cols`, and `chain_length` settings

### GPIO access denied
- Another process (ft-server) may be using GPIO
- Stop conflicting services before running
