cmake_minimum_required(VERSION 3.10)
project(audio_led VERSION 2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Use embedded Lua source (works for both native and cross-compilation)
set(LUA_SRC_DIR "${CMAKE_SOURCE_DIR}/libs/lua/src")
set(LUA_SOURCES
    ${LUA_SRC_DIR}/lapi.c
    ${LUA_SRC_DIR}/lauxlib.c
    ${LUA_SRC_DIR}/lbaselib.c
    ${LUA_SRC_DIR}/lbitlib.c
    ${LUA_SRC_DIR}/lcode.c
    ${LUA_SRC_DIR}/lcorolib.c
    ${LUA_SRC_DIR}/lctype.c
    ${LUA_SRC_DIR}/ldblib.c
    ${LUA_SRC_DIR}/ldebug.c
    ${LUA_SRC_DIR}/ldo.c
    ${LUA_SRC_DIR}/ldump.c
    ${LUA_SRC_DIR}/lfunc.c
    ${LUA_SRC_DIR}/lgc.c
    ${LUA_SRC_DIR}/linit.c
    ${LUA_SRC_DIR}/liolib.c
    ${LUA_SRC_DIR}/llex.c
    ${LUA_SRC_DIR}/lmathlib.c
    ${LUA_SRC_DIR}/lmem.c
    ${LUA_SRC_DIR}/loadlib.c
    ${LUA_SRC_DIR}/lobject.c
    ${LUA_SRC_DIR}/lopcodes.c
    ${LUA_SRC_DIR}/loslib.c
    ${LUA_SRC_DIR}/lparser.c
    ${LUA_SRC_DIR}/lstate.c
    ${LUA_SRC_DIR}/lstring.c
    ${LUA_SRC_DIR}/lstrlib.c
    ${LUA_SRC_DIR}/ltable.c
    ${LUA_SRC_DIR}/ltablib.c
    ${LUA_SRC_DIR}/ltm.c
    ${LUA_SRC_DIR}/lundump.c
    ${LUA_SRC_DIR}/lutf8lib.c
    ${LUA_SRC_DIR}/lvm.c
    ${LUA_SRC_DIR}/lzio.c
)
set(LUA_INCLUDE_DIRS ${LUA_SRC_DIR})
message(STATUS "Using embedded Lua 5.3 source")

# Cross-compilation detection
if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compiling for ${CMAKE_SYSTEM_PROCESSOR}")
    set(ALSA_LIBRARIES asound)
    set(ALSA_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/libs")
    message(STATUS "Using ALSA: ${ALSA_LIBRARIES}")
else()
    # Native compilation - use pkg-config for ALSA
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA REQUIRED alsa)
endif()

# RGB Matrix library path
set(RGB_MATRIX_PATH "${CMAKE_SOURCE_DIR}/../rpi-rgb-led-matrix")

# Source files
set(SOURCES
    src/main.cpp
    src/effect_manager.cpp
    src/lua_effect.cpp
    src/audio_capture.cpp
    src/web_server.cpp
    kissfft/kiss_fft.c
    ${LUA_SOURCES}
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/effects/builtin
    ${CMAKE_SOURCE_DIR}/kissfft
    ${RGB_MATRIX_PATH}/include
    ${ALSA_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIRS}
)

# Link directories
link_directories(
    ${CMAKE_SOURCE_DIR}/libs/arm
    ${RGB_MATRIX_PATH}/lib
)

# Create executable
add_executable(audio_led ${SOURCES})

# Link libraries
target_link_libraries(audio_led
    rgbmatrix
    ${ALSA_LIBRARIES}
    pthread
    m
    dl
)

# Install targets
install(TARGETS audio_led DESTINATION bin)
install(DIRECTORY effects/scripts/ DESTINATION share/audio_led/scripts)
install(DIRECTORY presets/ DESTINATION share/audio_led/presets)

# Copy scripts to build directory for development
file(COPY effects/scripts DESTINATION ${CMAKE_BINARY_DIR}/effects)
