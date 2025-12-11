# Cross-Compile Notes for Pi Zero 2 (64-bit)

## Status
- **Target**: Raspberry Pi Zero 2 W running **64-bit OS** (`uname -m` = `aarch64`)
- **Toolchain needed**: `aarch64-linux-gnu-gcc` (NOT `arm-linux-gnueabihf-gcc`)

## Completed
- [x] aarch64 toolchain installed in WSL (`gcc-aarch64-linux-gnu`, `g++-aarch64-linux-gnu`)
- [x] `cmake/aarch64-toolchain.cmake` already exists and is correct
- [x] `rpi-rgb-led-matrix` rebuilt for aarch64

## TODO
1. ~~**Get libasound for aarch64**~~ ✅ Done - libs in `libs/aarch64/`

2. ~~**Update CMakeLists.txt**~~ ✅ Done - Auto-detects architecture via `CMAKE_SYSTEM_PROCESSOR`

3. **Rebuild**:
   ```bash
   cd /mnt/d/Developer/C++/raspi
   rm -rf build-arm
   mkdir build-arm && cd build-arm
   cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-toolchain.cmake
   make -j$(nproc)
   ```

4. **Verify binary**:
   ```bash
   file audio_led
   # Should show: ELF 64-bit LSB executable, ARM aarch64
   ```

5. **Deploy to Pi and test**

## Error Reference
The previous build failed because:
- `librgbmatrix.a` was 32-bit → Fixed by rebuilding
- `libasound.so` in `libs/arm/` is 32-bit → Need aarch64 version
