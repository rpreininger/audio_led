// ====================================================================
// EFFECT MANAGER - Implementation
// ====================================================================
#include "effect_manager.h"
#include "lua_effect.h"
#include <iostream>
#include <dirent.h>
#include <algorithm>

EffectManager::EffectManager(int width, int height)
    : m_width(width), m_height(height) {
}

EffectManager::~EffectManager() = default;

void EffectManager::registerEffect(std::unique_ptr<Effect> effect) {
    effect->init(m_width, m_height);
    m_effects.push_back(std::move(effect));
    std::cerr << "Registered effect: " << m_effects.back()->getName() << std::endl;
}

void EffectManager::loadLuaEffects(const std::string& scriptsDir) {
    m_scriptsDir = scriptsDir;
    std::cerr << "Lua scripts directory set to: " << scriptsDir << std::endl;
}

Effect* EffectManager::getEffect(int index) {
    if (index >= 0 && index < static_cast<int>(m_effects.size())) {
        return m_effects[index].get();
    }
    return nullptr;
}

Effect* EffectManager::getEffectByName(const std::string& name) {
    for (auto& effect : m_effects) {
        if (effect->getName() == name) {
            return effect.get();
        }
    }
    return nullptr;
}

std::vector<std::string> EffectManager::getEffectNames() const {
    std::vector<std::string> names;
    for (const auto& effect : m_effects) {
        names.push_back(effect->getName());
    }
    return names;
}

void EffectManager::reloadLuaEffects() {
    std::cerr << "Reloading Lua effects from: " << m_scriptsDir << std::endl;

    // Remove all Lua effects (keep builtin C++ effects)
    if (m_builtinCount < m_effects.size()) {
        m_effects.resize(m_builtinCount);
        std::cerr << "  Removed old Lua effects, keeping " << m_builtinCount << " builtins" << std::endl;
    }

    // Rescan scripts directory
    DIR* dir = opendir(m_scriptsDir.c_str());
    if (!dir) {
        std::cerr << "  WARNING: Could not open scripts directory" << std::endl;
        return;
    }

    int luaCount = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        if (filename.length() > 4 &&
            filename.substr(filename.length() - 4) == ".lua") {
            std::string path = m_scriptsDir + "/" + filename;
            std::cerr << "  Loading: " << filename << std::endl;
            auto effect = std::make_unique<LuaEffect>(path);
            effect->init(m_width, m_height);
            if (effect->isValid()) {
                m_effects.push_back(std::move(effect));
                luaCount++;
            } else {
                std::cerr << "  FAILED: " << filename << std::endl;
            }
        }
    }
    closedir(dir);
    std::cerr << "Reloaded " << luaCount << " Lua effects (total: " << m_effects.size() << ")" << std::endl;
}
