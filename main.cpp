#include <iostream>
#include <fstream>
#include <string>

#include <lua.hpp>
#include "lua_api.hpp"
#include "libs/nlohmann_json.hpp"

using json = nlohmann::json;

int main() {
    // JSON laden
    std::ifstream f("preset.json");
    if (!f) { std::cerr << "preset.json fehlt!\n"; return 1; }

    json preset = json::parse(f);
    std::string script = preset["script"];

    // Auflösung übernehmen
    FB_WIDTH  = preset["resolution"][0];
    FB_HEIGHT = preset["resolution"][1];

    // Lua starten
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    // API registrieren
    lua_register(L, "drawPixel", lua_drawPixel);
    lua_register(L, "drawPixelHSV", lua_drawPixelHSV);

    // Lua-Skript laden
    if (luaL_dostring(L, script.c_str()) != LUA_OK) {
        std::cerr << lua_tostring(L, -1) << std::endl;
        return 1;
    }

    // init(params) aufrufen
    lua_getglobal(L, "init");
    pushParameters(L);
    lua_pcall(L, 1, 0, 0);

    float t = 0.0f;

    // Endlosschleife (z. B. 60 FPS)
    while (true) {
        // Audio-Beispielwerte (echte FFT könnt ihr später einbauen)
        float bass = (std::sin(t*4) + 1) * 0.5f;

        lua_getglobal(L, "update");
        pushAudio(L, bass, 0.2, 0.1, bass);
        lua_pushnumber(L, t);
        lua_pcall(L, 2, 0, 0);

        // TODO: framebuffer auf echte LED-Matrix ausgeben

        t += 0.016;
        usleep(16000);
    }

    lua_close(L);
    return 0;
}
