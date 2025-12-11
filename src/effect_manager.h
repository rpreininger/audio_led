// ====================================================================
// EFFECT MANAGER - Manages builtin and Lua effects
// ====================================================================
#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>
#include "effect.h"

class EffectManager {
public:
    EffectManager(int width, int height);
    ~EffectManager();

    // Register a builtin C++ effect
    void registerEffect(std::unique_ptr<Effect> effect);

    // Load Lua effects from scripts directory
    void loadLuaEffects(const std::string& scriptsDir);

    // Get effect by index
    Effect* getEffect(int index);

    // Get effect by name
    Effect* getEffectByName(const std::string& name);

    // Get number of effects
    int getEffectCount() const { return static_cast<int>(m_effects.size()); }

    // Get effect names for UI
    std::vector<std::string> getEffectNames() const;

    // Reload Lua effects (hot reload)
    void reloadLuaEffects();

    // Mark where builtin effects end (for reload)
    void markBuiltinEnd() { m_builtinCount = m_effects.size(); }

private:
    int m_width;
    int m_height;
    std::vector<std::unique_ptr<Effect>> m_effects;
    std::string m_scriptsDir;
    size_t m_builtinCount = 0;  // Number of C++ builtin effects
};
