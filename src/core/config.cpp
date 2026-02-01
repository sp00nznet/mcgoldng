#include "core/config.h"
#include "assets/fit_parser.h"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace mcgng {

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    loadDefaults();
}

void ConfigManager::loadDefaults() {
    m_config = GameConfig{};

    // Try to determine default paths
#ifdef _WIN32
    // Windows: use AppData for saves
    if (const char* appdata = std::getenv("APPDATA")) {
        m_config.savePath = std::string(appdata) + "\\MCGoldNG\\saves";
        m_configPath = std::string(appdata) + "\\MCGoldNG\\config.cfg";
    }
#else
    // Unix: use home directory
    if (const char* home = std::getenv("HOME")) {
        m_config.savePath = std::string(home) + "/.mcgoldng/saves";
        m_configPath = std::string(home) + "/.mcgoldng/config.cfg";
    }
#endif
}

bool ConfigManager::load(const std::string& path) {
    FitParser parser;

    if (!parser.parseFile(path)) {
        std::cerr << "ConfigManager: Failed to load config: " << parser.getError() << "\n";
        return false;
    }

    // Load Display settings
    if (const auto* display = parser.findBlock("Display")) {
        if (auto val = display->getInt("WindowWidth")) m_config.windowWidth = static_cast<int>(*val);
        if (auto val = display->getInt("WindowHeight")) m_config.windowHeight = static_cast<int>(*val);
        if (auto val = display->getBool("Fullscreen")) m_config.fullscreen = *val;
        if (auto val = display->getBool("VSync")) m_config.vsync = *val;
        if (auto val = display->getInt("TargetFPS")) m_config.targetFPS = static_cast<int>(*val);
    }

    // Load Graphics settings
    if (const auto* graphics = parser.findBlock("Graphics")) {
        if (auto val = graphics->getInt("RenderScale")) m_config.renderScale = static_cast<int>(*val);
        if (auto val = graphics->getBool("SmoothScaling")) m_config.smoothScaling = *val;
        if (auto val = graphics->getBool("ShowFPS")) m_config.showFPS = *val;
    }

    // Load Audio settings
    if (const auto* audio = parser.findBlock("Audio")) {
        if (auto val = audio->getInt("MasterVolume")) m_config.masterVolume = static_cast<int>(*val);
        if (auto val = audio->getInt("MusicVolume")) m_config.musicVolume = static_cast<int>(*val);
        if (auto val = audio->getInt("SFXVolume")) m_config.sfxVolume = static_cast<int>(*val);
        if (auto val = audio->getInt("VoiceVolume")) m_config.voiceVolume = static_cast<int>(*val);
        if (auto val = audio->getBool("MuteAudio")) m_config.muteAudio = *val;
    }

    // Load Paths
    if (const auto* paths = parser.findBlock("Paths")) {
        if (auto val = paths->getString("GamePath")) m_config.gamePath = *val;
        if (auto val = paths->getString("AssetsPath")) m_config.assetsPath = *val;
        if (auto val = paths->getString("SavePath")) m_config.savePath = *val;
    }

    // Load Gameplay settings
    if (const auto* gameplay = parser.findBlock("Gameplay")) {
        if (auto val = gameplay->getFloat("GameSpeed")) m_config.gameSpeed = static_cast<float>(*val);
        if (auto val = gameplay->getBool("PauseOnFocusLoss")) m_config.pauseOnFocusLoss = *val;
        if (auto val = gameplay->getInt("Difficulty")) m_config.difficulty = static_cast<int>(*val);
    }

    // Load Debug settings
    if (const auto* debug = parser.findBlock("Debug")) {
        if (auto val = debug->getBool("DebugMode")) m_config.debugMode = *val;
        if (auto val = debug->getBool("ShowCollision")) m_config.showCollision = *val;
        if (auto val = debug->getBool("ShowPathfinding")) m_config.showPathfinding = *val;
    }

    m_configPath = path;
    return true;
}

bool ConfigManager::save(const std::string& path) const {
    // Create parent directories if needed
    fs::path configPath(path);
    if (configPath.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(configPath.parent_path(), ec);
    }

    std::ofstream file(path);
    if (!file) {
        std::cerr << "ConfigManager: Failed to create config file: " << path << "\n";
        return false;
    }

    file << "FITini\n\n";

    // Display settings
    file << "[Display]\n";
    file << "l WindowWidth = " << m_config.windowWidth << "\n";
    file << "l WindowHeight = " << m_config.windowHeight << "\n";
    file << "b Fullscreen = " << (m_config.fullscreen ? "TRUE" : "FALSE") << "\n";
    file << "b VSync = " << (m_config.vsync ? "TRUE" : "FALSE") << "\n";
    file << "l TargetFPS = " << m_config.targetFPS << "\n";
    file << "\n";

    // Graphics settings
    file << "[Graphics]\n";
    file << "l RenderScale = " << m_config.renderScale << "\n";
    file << "b SmoothScaling = " << (m_config.smoothScaling ? "TRUE" : "FALSE") << "\n";
    file << "b ShowFPS = " << (m_config.showFPS ? "TRUE" : "FALSE") << "\n";
    file << "\n";

    // Audio settings
    file << "[Audio]\n";
    file << "l MasterVolume = " << m_config.masterVolume << "\n";
    file << "l MusicVolume = " << m_config.musicVolume << "\n";
    file << "l SFXVolume = " << m_config.sfxVolume << "\n";
    file << "l VoiceVolume = " << m_config.voiceVolume << "\n";
    file << "b MuteAudio = " << (m_config.muteAudio ? "TRUE" : "FALSE") << "\n";
    file << "\n";

    // Paths
    file << "[Paths]\n";
    file << "st GamePath = \"" << m_config.gamePath << "\"\n";
    file << "st AssetsPath = \"" << m_config.assetsPath << "\"\n";
    file << "st SavePath = \"" << m_config.savePath << "\"\n";
    file << "\n";

    // Gameplay settings
    file << "[Gameplay]\n";
    file << "f GameSpeed = " << m_config.gameSpeed << "\n";
    file << "b PauseOnFocusLoss = " << (m_config.pauseOnFocusLoss ? "TRUE" : "FALSE") << "\n";
    file << "l Difficulty = " << m_config.difficulty << "\n";
    file << "\n";

    // Debug settings
    file << "[Debug]\n";
    file << "b DebugMode = " << (m_config.debugMode ? "TRUE" : "FALSE") << "\n";
    file << "b ShowCollision = " << (m_config.showCollision ? "TRUE" : "FALSE") << "\n";
    file << "b ShowPathfinding = " << (m_config.showPathfinding ? "TRUE" : "FALSE") << "\n";
    file << "\n";

    file << "FITend\n";

    return !file.fail();
}

std::optional<ConfigValue> ConfigManager::getValue(const std::string& key) const {
    auto it = m_extras.find(key);
    if (it != m_extras.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ConfigManager::setValue(const std::string& key, const ConfigValue& value) {
    m_extras[key] = value;
}

} // namespace mcgng
