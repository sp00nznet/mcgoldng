#ifndef MCGNG_CONFIG_H
#define MCGNG_CONFIG_H

#include <string>
#include <variant>
#include <map>
#include <optional>

namespace mcgng {

/**
 * Configuration value type.
 */
using ConfigValue = std::variant<bool, int64_t, double, std::string>;

/**
 * Game configuration settings.
 */
struct GameConfig {
    // Display settings
    int windowWidth = 1280;
    int windowHeight = 720;
    bool fullscreen = false;
    bool vsync = true;
    int targetFPS = 60;

    // Graphics settings
    int renderScale = 100;      // Percentage (100 = native)
    bool smoothScaling = true;
    bool showFPS = false;

    // Audio settings
    int masterVolume = 100;
    int musicVolume = 80;
    int sfxVolume = 100;
    int voiceVolume = 100;
    bool muteAudio = false;

    // Paths
    std::string gamePath;       // Path to original MCG installation
    std::string assetsPath;     // Path to extracted assets
    std::string savePath;       // Path to save games

    // Gameplay
    float gameSpeed = 1.0f;
    bool pauseOnFocusLoss = true;
    int difficulty = 1;         // 0=Easy, 1=Normal, 2=Hard

    // Debug
    bool debugMode = false;
    bool showCollision = false;
    bool showPathfinding = false;
};

/**
 * Configuration manager.
 * Handles loading, saving, and accessing game settings.
 */
class ConfigManager {
public:
    static ConfigManager& instance();

    /**
     * Load configuration from file.
     * @param path Path to config file
     * @return true on success
     */
    bool load(const std::string& path);

    /**
     * Save configuration to file.
     * @param path Path to config file
     * @return true on success
     */
    bool save(const std::string& path) const;

    /**
     * Load default configuration.
     */
    void loadDefaults();

    /**
     * Get current configuration.
     */
    GameConfig& get() { return m_config; }
    const GameConfig& get() const { return m_config; }

    /**
     * Get a named value from extras.
     */
    std::optional<ConfigValue> getValue(const std::string& key) const;

    /**
     * Set a named value in extras.
     */
    void setValue(const std::string& key, const ConfigValue& value);

    /**
     * Get config file path.
     */
    const std::string& getConfigPath() const { return m_configPath; }

private:
    ConfigManager();
    ~ConfigManager() = default;

    GameConfig m_config;
    std::map<std::string, ConfigValue> m_extras;
    std::string m_configPath;
};

} // namespace mcgng

#endif // MCGNG_CONFIG_H
