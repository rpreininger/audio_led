// ====================================================================
// EFFECT MANAGER - Implementation
// ====================================================================
#include "effect_manager.h"
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
    // TODO: Implement Lua effect loading
    std::cerr << "Lua effects will be loaded from: " << scriptsDir << std::endl;
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
    // TODO: Implement hot reload of Lua effects
    std::cerr << "Reloading Lua effects..." << std::endl;
}
