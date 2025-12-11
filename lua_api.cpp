#include "lua_api.hpp"
#include <cmath>
#include <iostream>

int FB_WIDTH = 64;
int FB_HEIGHT = 128;

// Framebuffer [x][y][RGB]
unsigned char framebuffer[64][128][3];

void hsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    float c = v * s;
    float x = c * (1 - fabs(fmod(h / 60.0, 2) - 1));
    float m = v - c;

    float r1, g1, b1;

    if (h < 60)      { r1 = c; g1 = x; b1 = 0; }
    else if (h < 120){ r1 = x; g1 = c; b1 = 0; }
    else if (h < 180){ r1 = 0; g1 = c; b1 = x; }
    else if (h < 240){ r1 = 0; g1 = x; b1 = c; }
    else if (h < 300){ r1 = x; g1 = 0; b1 = c; }
    else             { r1 = c; g1 = 0; b1 = x; }

    r = (r1 + m);
    g = (g1 + m);
    b = (b1 + m);
}

// drawPixel(x, y, r, g, b)
int lua_drawPixel(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    float r = luaL_checknumber(L, 3);
    float g = luaL_checknumber(L, 4);
    float b = luaL_checknumber(L, 5);

    if (x>=0 && x<FB_WIDTH && y>=0 && y<FB_HEIGHT) {
        framebuffer[x][y][0] = (unsigned char)(r * 255);
        framebuffer[x][y][1] = (unsigned char)(g * 255);
        framebuffer[x][y][2] = (unsigned char)(b * 255);
    }
    return 0;
}

// drawPixelHSV(x, y, h, s, v)
int lua_drawPixelHSV(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    float h = luaL_checknumber(L, 3);
    float s = luaL_checknumber(L, 4);
    float v = luaL_checknumber(L, 5);

    float r, g, b;
    hsvToRgb(h, s, v, r, g, b);

    if (x>=0 && x<FB_WIDTH && y>=0 && y<FB_HEIGHT) {
        framebuffer[x][y][0] = (unsigned char)(r * 255);
        framebuffer[x][y][1] = (unsigned char)(g * 255);
        framebuffer[x][y][2] = (unsigned char)(b * 255);
    }
    return 0;
}

// Übergibt die JSON-Parameter an init()
void pushParameters(lua_State* L) {
    lua_newtable(L);
    int t = lua_gettop(L);

    lua_pushnumber(L, 0.3);
    lua_setfield(L, t, "hue_speed");

    lua_pushnumber(L, 25);
    lua_setfield(L, t, "radius_scale");
}

// Übergibt Audiodaten an update()
void pushAudio(lua_State* L, float bass, float mid, float treble, float level) {
    lua_newtable(L);
    int t = lua_gettop(L);

    lua_pushnumber(L, bass);   lua_setfield(L, t, "bass");
    lua_pushnumber(L, mid);    lua_setfield(L, t, "mid");
    lua_pushnumber(L, treble); lua_setfield(L, t, "treble");
    lua_pushnumber(L, level);  lua_setfield(L, t, "level");
}
