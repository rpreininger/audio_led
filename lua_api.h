#pragma once
#include <lua.hpp>

// Framebuffer-Größe (wird optional durch JSON überschrieben)
extern int FB_WIDTH;
extern int FB_HEIGHT;

// Unser Framebuffer
extern unsigned char framebuffer[64][128][3];

// Zeichenfunktionen für Lua
int lua_drawPixel(lua_State* L);
int lua_drawPixelHSV(lua_State* L);

// Farbkonvertierung
void hsvToRgb(float h, float s, float v, float& r, float& g, float& b);

// Hilfsfunktionen zum Übergeben von JSON-Parametern & Audiodaten
void pushParameters(lua_State* L);
void pushAudio(lua_State* L, float bass, float mid, float treble, float level);
