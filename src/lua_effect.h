// ====================================================================
// LUA EFFECT - Wrapper for Lua scripted effects
// ====================================================================
#pragma once

#include "effect.h"
#include <string>

// Forward declaration for Lua state
struct lua_State;

class LuaEffect : public Effect {
public:
    LuaEffect(const std::string& scriptPath);
    ~LuaEffect();

    std::string getName() const override { return m_name; }
    std::string getDescription() const override { return m_description; }

    void init(int width, int height) override;
    void update(rgb_matrix::FrameCanvas* canvas,
               const AudioData& audio,
               const EffectSettings& settings,
               float time) override;
    void reset() override;

    // Check if script loaded successfully
    bool isValid() const { return m_valid; }

    // Reload script from file
    bool reload();

private:
    std::string m_scriptPath;
    std::string m_name;
    std::string m_description;
    lua_State* m_lua;
    bool m_valid;

    // Framebuffer for Lua to write to
    unsigned char m_framebuffer[128][64][3];

    // Register Lua API functions
    void registerAPI();

    // Push audio data to Lua
    void pushAudioTable(const AudioData& audio);

    // Push settings to Lua
    void pushSettingsTable(const EffectSettings& settings);

    // Copy framebuffer to canvas
    void copyToCanvas(rgb_matrix::FrameCanvas* canvas, int brightness);

    // Static callbacks for Lua
    static int lua_setPixel(lua_State* L);
    static int lua_setPixelHSV(lua_State* L);
    static int lua_clear(lua_State* L);
    static int lua_drawLine(lua_State* L);
    static int lua_drawRect(lua_State* L);
    static int lua_fillRect(lua_State* L);
    static int lua_drawCircle(lua_State* L);
    static int lua_fillCircle(lua_State* L);
    static int lua_getWidth(lua_State* L);
    static int lua_getHeight(lua_State* L);
};
