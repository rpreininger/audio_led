cmake_minimum_required(VERSION 3.10)
project(audio_led VERSION 2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Find required packages
find_package(PkgConfig REQUIRED)

# Find ALSA
pkg_check_modules(ALSA REQUIRED alsa)

# Find Lua (prefer LuaJIT if available)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUAJIT luajit)
if(LUAJIT_FOUND)
    message(STATUS "Using LuaJIT for better performance")
    set(LUA_INCLUDE_DIRS ${LUAJIT_INCLUDE_DIRS})
    set(LUA_LIBRARIES ${LUAJIT_LIBRARIES})
else()
    pkg_check_modules(LUA REQUIRED lua5.3)
    set(LUA_INCLUDE_DIRS ${LUA_INCLUDE_DIRS})
    set(LUA_LIBRARIES ${LUA_LIBRARIES})
    message(STATUS "Using standard Lua 5.3")
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
    ${RGB_MATRIX_PATH}/lib
)

# Create executable
add_executable(audio_led ${SOURCES})

# Link libraries
target_link_libraries(audio_led
    rgbmatrix
    ${ALSA_LIBRARIES}
    ${LUA_LIBRARIES}
    pthread
)

# Install targets
install(TARGETS audio_led DESTINATION bin)
install(DIRECTORY effects/scripts/ DESTINATION share/audio_led/scripts)
install(DIRECTORY presets/ DESTINATION share/audio_led/presets)

# Copy scripts to build directory for development
file(COPY effects/scripts DESTINATION ${CMAKE_BINARY_DIR}/effects)
