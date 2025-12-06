// ====================================================================
// LUA EFFECT - Implementation
// ====================================================================
#include "lua_effect.h"
#include "led-matrix.h"
#include <lua.hpp>
#include <cmath>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>

// Store pointer to current LuaEffect instance for static callbacks
static thread_local LuaEffect* g_currentEffect = nullptr;

LuaEffect::LuaEffect(const std::string& scriptPath)
    : m_scriptPath(scriptPath), m_lua(nullptr), m_valid(false) {

    // Extract name from filename
    size_t lastSlash = scriptPath.find_last_of("/\\");
    size_t lastDot = scriptPath.find_last_of(".");
    if (lastSlash == std::string::npos) lastSlash = 0;
    else lastSlash++;
    if (lastDot == std::string::npos) lastDot = scriptPath.length();
    m_name = scriptPath.substr(lastSlash, lastDot - lastSlash);

    m_description = "Lua script: " + m_name;

    // Clear framebuffer
    memset(m_framebuffer, 0, sizeof(m_framebuffer));
}

LuaEffect::~LuaEffect() {
    if (m_lua) {
        lua_close(m_lua);
    }
}

void LuaEffect::init(int width, int height) {
    Effect::init(width, height);
    reload();
}

bool LuaEffect::reload() {
    // Close existing Lua state
    if (m_lua) {
        lua_close(m_lua);
        m_lua = nullptr;
    }
    m_valid = false;

    // Create new Lua state
    m_lua = luaL_newstate();
    if (!m_lua) {
        std::cerr << "Failed to create Lua state for: " << m_name << std::endl;
        return false;
    }

    // Open standard libraries
    luaL_openlibs(m_lua);

    // Register our API
    registerAPI();

    // Load and run script
    if (luaL_dofile(m_lua, m_scriptPath.c_str()) != LUA_OK) {
        std::cerr << "Lua error in " << m_name << ": "
                  << lua_tostring(m_lua, -1) << std::endl;
        lua_pop(m_lua, 1);
        return false;
    }

    // Try to get effect name from script
    lua_getglobal(m_lua, "effect_name");
    if (lua_isstring(m_lua, -1)) {
        m_name = lua_tostring(m_lua, -1);
    }
    lua_pop(m_lua, 1);

    // Try to get description
    lua_getglobal(m_lua, "effect_description");
    if (lua_isstring(m_lua, -1)) {
        m_description = lua_tostring(m_lua, -1);
    }
    lua_pop(m_lua, 1);

    // Call init function if it exists
    lua_getglobal(m_lua, "init");
    if (lua_isfunction(m_lua, -1)) {
        lua_pushinteger(m_lua, m_width);
        lua_pushinteger(m_lua, m_height);
        if (lua_pcall(m_lua, 2, 0, 0) != LUA_OK) {
            std::cerr << "Lua init error: " << lua_tostring(m_lua, -1) << std::endl;
            lua_pop(m_lua, 1);
        }
    } else {
        lua_pop(m_lua, 1);
    }

    m_valid = true;
    std::cerr << "Loaded Lua effect: " << m_name << std::endl;
    return true;
}

void LuaEffect::registerAPI() {
    // Store this pointer in Lua registry
    lua_pushlightuserdata(m_lua, this);
    lua_setglobal(m_lua, "__effect_ptr");

    // Register drawing functions
    lua_register(m_lua, "setPixel", lua_setPixel);
    lua_register(m_lua, "setPixelHSV", lua_setPixelHSV);
    lua_register(m_lua, "clear", lua_clear);
    lua_register(m_lua, "drawLine", lua_drawLine);
    lua_register(m_lua, "drawRect", lua_drawRect);
    lua_register(m_lua, "fillRect", lua_fillRect);
    lua_register(m_lua, "drawCircle", lua_drawCircle);
    lua_register(m_lua, "fillCircle", lua_fillCircle);
    lua_register(m_lua, "getWidth", lua_getWidth);
    lua_register(m_lua, "getHeight", lua_getHeight);

    // Set width/height globals
    lua_pushinteger(m_lua, m_width);
    lua_setglobal(m_lua, "WIDTH");
    lua_pushinteger(m_lua, m_height);
    lua_setglobal(m_lua, "HEIGHT");
}

void LuaEffect::update(rgb_matrix::FrameCanvas* canvas,
                       const AudioData& audio,
                       const EffectSettings& settings,
                       float time) {
    if (!m_valid || !m_lua) return;

    g_currentEffect = this;

    // Call update function
    lua_getglobal(m_lua, "update");
    if (!lua_isfunction(m_lua, -1)) {
        lua_pop(m_lua, 1);
        g_currentEffect = nullptr;
        return;
    }

    // Push audio table
    pushAudioTable(audio);

    // Push settings table
    pushSettingsTable(settings);

    // Push time
    lua_pushnumber(m_lua, time);

    // Call update(audio, settings, time)
    if (lua_pcall(m_lua, 3, 0, 0) != LUA_OK) {
        std::cerr << "Lua update error: " << lua_tostring(m_lua, -1) << std::endl;
        lua_pop(m_lua, 1);
    }

    // Copy framebuffer to canvas
    copyToCanvas(canvas, settings.brightness);

    g_currentEffect = nullptr;
}

void LuaEffect::reset() {
    if (!m_valid || !m_lua) return;

    lua_getglobal(m_lua, "reset");
    if (lua_isfunction(m_lua, -1)) {
        if (lua_pcall(m_lua, 0, 0, 0) != LUA_OK) {
            lua_pop(m_lua, 1);
        }
    } else {
        lua_pop(m_lua, 1);
    }

    memset(m_framebuffer, 0, sizeof(m_framebuffer));
}

void LuaEffect::pushAudioTable(const AudioData& audio) {
    lua_newtable(m_lua);

    lua_pushnumber(m_lua, audio.volume);
    lua_setfield(m_lua, -2, "volume");

    lua_pushnumber(m_lua, audio.beat);
    lua_setfield(m_lua, -2, "beat");

    lua_pushnumber(m_lua, audio.bass);
    lua_setfield(m_lua, -2, "bass");

    lua_pushnumber(m_lua, audio.mid);
    lua_setfield(m_lua, -2, "mid");

    lua_pushnumber(m_lua, audio.treble);
    lua_setfield(m_lua, -2, "treble");

    // Spectrum array
    lua_newtable(m_lua);
    for (int i = 0; i < 8; i++) {
        lua_pushnumber(m_lua, audio.spectrum[i]);
        lua_rawseti(m_lua, -2, i + 1);  // Lua arrays are 1-indexed
    }
    lua_setfield(m_lua, -2, "spectrum");
}

void LuaEffect::pushSettingsTable(const EffectSettings& settings) {
    lua_newtable(m_lua);

    lua_pushinteger(m_lua, settings.brightness);
    lua_setfield(m_lua, -2, "brightness");

    lua_pushnumber(m_lua, settings.sensitivity);
    lua_setfield(m_lua, -2, "sensitivity");

    lua_pushnumber(m_lua, settings.noiseThreshold);
    lua_setfield(m_lua, -2, "noiseThreshold");
}

void LuaEffect::copyToCanvas(rgb_matrix::FrameCanvas* canvas, int brightness) {
    float scale = brightness / 255.0f;
    for (int y = 0; y < m_height; y++) {
        for (int x = 0; x < m_width; x++) {
            int r = static_cast<int>(m_framebuffer[x][y][0] * scale);
            int g = static_cast<int>(m_framebuffer[x][y][1] * scale);
            int b = static_cast<int>(m_framebuffer[x][y][2] * scale);
            canvas->SetPixel(x, y, r, g, b);
        }
    }
}

// Helper to get effect pointer from Lua state
static LuaEffect* getEffect(lua_State* L) {
    return g_currentEffect;
}

// HSV to RGB conversion
static void hsvToRgb(float h, float s, float v, int& r, int& g, int& b) {
    float c = v * s;
    float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;
    float r1, g1, b1;

    if (h < 60)       { r1 = c; g1 = x; b1 = 0; }
    else if (h < 120) { r1 = x; g1 = c; b1 = 0; }
    else if (h < 180) { r1 = 0; g1 = c; b1 = x; }
    else if (h < 240) { r1 = 0; g1 = x; b1 = c; }
    else if (h < 300) { r1 = x; g1 = 0; b1 = c; }
    else              { r1 = c; g1 = 0; b1 = x; }

    r = static_cast<int>((r1 + m) * 255);
    g = static_cast<int>((g1 + m) * 255);
    b = static_cast<int>((b1 + m) * 255);
}

// Lua API: setPixel(x, y, r, g, b)
int LuaEffect::lua_setPixel(lua_State* L) {
    LuaEffect* effect = getEffect(L);
    if (!effect) return 0;

    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int r = static_cast<int>(luaL_checknumber(L, 3) * 255);
    int g = static_cast<int>(luaL_checknumber(L, 4) * 255);
    int b = static_cast<int>(luaL_checknumber(L, 5) * 255);

    if (x >= 0 && x < effect->m_width && y >= 0 && y < effect->m_height) {
        effect->m_framebuffer[x][y][0] = r;
        effect->m_framebuffer[x][y][1] = g;
        effect->m_framebuffer[x][y][2] = b;
    }
    return 0;
}

// Lua API: setPixelHSV(x, y, h, s, v)
int LuaEffect::lua_setPixelHSV(lua_State* L) {
    LuaEffect* effect = getEffect(L);
    if (!effect) return 0;

    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    float h = luaL_checknumber(L, 3);
    float s = luaL_checknumber(L, 4);
    float v = luaL_checknumber(L, 5);

    int r, g, b;
    hsvToRgb(h, s, v, r, g, b);

    if (x >= 0 && x < effect->m_width && y >= 0 && y < effect->m_height) {
        effect->m_framebuffer[x][y][0] = r;
        effect->m_framebuffer[x][y][1] = g;
        effect->m_framebuffer[x][y][2] = b;
    }
    return 0;
}

// Lua API: clear(r, g, b) or clear()
int LuaEffect::lua_clear(lua_State* L) {
    LuaEffect* effect = getEffect(L);
    if (!effect) return 0;

    int r = 0, g = 0, b = 0;
    if (lua_gettop(L) >= 3) {
        r = static_cast<int>(luaL_checknumber(L, 1) * 255);
        g = static_cast<int>(luaL_checknumber(L, 2) * 255);
        b = static_cast<int>(luaL_checknumber(L, 3) * 255);
    }

    for (int y = 0; y < effect->m_height; y++) {
        for (int x = 0; x < effect->m_width; x++) {
            effect->m_framebuffer[x][y][0] = r;
            effect->m_framebuffer[x][y][1] = g;
            effect->m_framebuffer[x][y][2] = b;
        }
    }
    return 0;
}

// Lua API: drawLine(x1, y1, x2, y2, r, g, b)
int LuaEffect::lua_drawLine(lua_State* L) {
    LuaEffect* effect = getEffect(L);
    if (!effect) return 0;

    int x1 = luaL_checkinteger(L, 1);
    int y1 = luaL_checkinteger(L, 2);
    int x2 = luaL_checkinteger(L, 3);
    int y2 = luaL_checkinteger(L, 4);
    int r = static_cast<int>(luaL_checknumber(L, 5) * 255);
    int g = static_cast<int>(luaL_checknumber(L, 6) * 255);
    int b = static_cast<int>(luaL_checknumber(L, 7) * 255);

    // Bresenham's line algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x1 >= 0 && x1 < effect->m_width && y1 >= 0 && y1 < effect->m_height) {
            effect->m_framebuffer[x1][y1][0] = r;
            effect->m_framebuffer[x1][y1][1] = g;
            effect->m_framebuffer[x1][y1][2] = b;
        }

        if (x1 == x2 && y1 == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
    return 0;
}

// Lua API: drawRect(x, y, w, h, r, g, b)
int LuaEffect::lua_drawRect(lua_State* L) {
    LuaEffect* effect = getEffect(L);
    if (!effect) return 0;

    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);
    int r = static_cast<int>(luaL_checknumber(L, 5) * 255);
    int g = static_cast<int>(luaL_checknumber(L, 6) * 255);
    int b = static_cast<int>(luaL_checknumber(L, 7) * 255);

    // Draw outline
    for (int i = x; i < x + w; i++) {
        if (i >= 0 && i < effect->m_width) {
            if (y >= 0 && y < effect->m_height) {
                effect->m_framebuffer[i][y][0] = r;
                effect->m_framebuffer[i][y][1] = g;
                effect->m_framebuffer[i][y][2] = b;
            }
            if (y + h - 1 >= 0 && y + h - 1 < effect->m_height) {
                effect->m_framebuffer[i][y + h - 1][0] = r;
                effect->m_framebuffer[i][y + h - 1][1] = g;
                effect->m_framebuffer[i][y + h - 1][2] = b;
            }
        }
    }
    for (int j = y; j < y + h; j++) {
        if (j >= 0 && j < effect->m_height) {
            if (x >= 0 && x < effect->m_width) {
                effect->m_framebuffer[x][j][0] = r;
                effect->m_framebuffer[x][j][1] = g;
                effect->m_framebuffer[x][j][2] = b;
            }
            if (x + w - 1 >= 0 && x + w - 1 < effect->m_width) {
                effect->m_framebuffer[x + w - 1][j][0] = r;
                effect->m_framebuffer[x + w - 1][j][1] = g;
                effect->m_framebuffer[x + w - 1][j][2] = b;
            }
        }
    }
    return 0;
}

// Lua API: fillRect(x, y, w, h, r, g, b)
int LuaEffect::lua_fillRect(lua_State* L) {
    LuaEffect* effect = getEffect(L);
    if (!effect) return 0;

    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);
    int r = static_cast<int>(luaL_checknumber(L, 5) * 255);
    int g = static_cast<int>(luaL_checknumber(L, 6) * 255);
    int b = static_cast<int>(luaL_checknumber(L, 7) * 255);

    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            if (i >= 0 && i < effect->m_width && j >= 0 && j < effect->m_height) {
                effect->m_framebuffer[i][j][0] = r;
                effect->m_framebuffer[i][j][1] = g;
                effect->m_framebuffer[i][j][2] = b;
            }
        }
    }
    return 0;
}

// Lua API: drawCircle(cx, cy, radius, r, g, b)
int LuaEffect::lua_drawCircle(lua_State* L) {
    LuaEffect* effect = getEffect(L);
    if (!effect) return 0;

    int cx = luaL_checkinteger(L, 1);
    int cy = luaL_checkinteger(L, 2);
    int radius = luaL_checkinteger(L, 3);
    int r = static_cast<int>(luaL_checknumber(L, 4) * 255);
    int g = static_cast<int>(luaL_checknumber(L, 5) * 255);
    int b = static_cast<int>(luaL_checknumber(L, 6) * 255);

    // Midpoint circle algorithm
    int x = radius;
    int y = 0;
    int err = 0;

    auto setPixel = [&](int px, int py) {
        if (px >= 0 && px < effect->m_width && py >= 0 && py < effect->m_height) {
            effect->m_framebuffer[px][py][0] = r;
            effect->m_framebuffer[px][py][1] = g;
            effect->m_framebuffer[px][py][2] = b;
        }
    };

    while (x >= y) {
        setPixel(cx + x, cy + y);
        setPixel(cx + y, cy + x);
        setPixel(cx - y, cy + x);
        setPixel(cx - x, cy + y);
        setPixel(cx - x, cy - y);
        setPixel(cx - y, cy - x);
        setPixel(cx + y, cy - x);
        setPixel(cx + x, cy - y);

        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
    return 0;
}

// Lua API: fillCircle(cx, cy, radius, r, g, b)
int LuaEffect::lua_fillCircle(lua_State* L) {
    LuaEffect* effect = getEffect(L);
    if (!effect) return 0;

    int cx = luaL_checkinteger(L, 1);
    int cy = luaL_checkinteger(L, 2);
    int radius = luaL_checkinteger(L, 3);
    int r = static_cast<int>(luaL_checknumber(L, 4) * 255);
    int g = static_cast<int>(luaL_checknumber(L, 5) * 255);
    int b = static_cast<int>(luaL_checknumber(L, 6) * 255);

    int r2 = radius * radius;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= r2) {
                int px = cx + dx;
                int py = cy + dy;
                if (px >= 0 && px < effect->m_width && py >= 0 && py < effect->m_height) {
                    effect->m_framebuffer[px][py][0] = r;
                    effect->m_framebuffer[px][py][1] = g;
                    effect->m_framebuffer[px][py][2] = b;
                }
            }
        }
    }
    return 0;
}

// Lua API: getWidth()
int LuaEffect::lua_getWidth(lua_State* L) {
    LuaEffect* effect = getEffect(L);
    lua_pushinteger(L, effect ? effect->m_width : 128);
    return 1;
}

// Lua API: getHeight()
int LuaEffect::lua_getHeight(lua_State* L) {
    LuaEffect* effect = getEffect(L);
    lua_pushinteger(L, effect ? effect->m_height : 64);
    return 1;
}
