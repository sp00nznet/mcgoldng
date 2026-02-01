#ifndef MCGNG_ENGINE_H
#define MCGNG_ENGINE_H

#include <cstdint>
#include <string>
#include <memory>
#include <functional>

namespace mcgng {

// Forward declarations
class ConfigManager;

/**
 * Engine state enumeration.
 */
enum class EngineState {
    Uninitialized,
    Initializing,
    Running,
    Paused,
    ShuttingDown,
    Terminated
};

/**
 * Main game loop callback types.
 */
using UpdateCallback = std::function<void(float deltaTime)>;
using RenderCallback = std::function<void()>;
using EventCallback = std::function<bool()>;  // Return false to quit

/**
 * Engine initialization options.
 */
struct EngineOptions {
    std::string windowTitle = "MechCommander Gold: Next Generation";
    std::string configPath;         // Path to config file (optional)
    std::string assetsPath;         // Path to extracted assets
    bool headless = false;          // Run without graphics (for tools)
};

/**
 * Main game engine class.
 *
 * Manages the game loop, subsystem initialization, and resource coordination.
 */
class Engine {
public:
    static Engine& instance();

    /**
     * Initialize the engine with the given options.
     * @param options Engine initialization options
     * @return true on success
     */
    bool initialize(const EngineOptions& options);

    /**
     * Shutdown the engine and clean up resources.
     */
    void shutdown();

    /**
     * Run the main game loop.
     * This function blocks until the game exits.
     */
    void run();

    /**
     * Request the engine to quit.
     */
    void quit();

    /**
     * Pause the game loop.
     */
    void pause();

    /**
     * Resume from pause.
     */
    void resume();

    /**
     * Check if engine is running.
     */
    bool isRunning() const { return m_state == EngineState::Running; }

    /**
     * Check if engine is paused.
     */
    bool isPaused() const { return m_state == EngineState::Paused; }

    /**
     * Get current engine state.
     */
    EngineState getState() const { return m_state; }

    /**
     * Set the update callback.
     */
    void setUpdateCallback(UpdateCallback callback) { m_updateCallback = std::move(callback); }

    /**
     * Set the render callback.
     */
    void setRenderCallback(RenderCallback callback) { m_renderCallback = std::move(callback); }

    /**
     * Set the event callback.
     */
    void setEventCallback(EventCallback callback) { m_eventCallback = std::move(callback); }

    /**
     * Get delta time from last frame.
     */
    float getDeltaTime() const { return m_deltaTime; }

    /**
     * Get total elapsed time since start.
     */
    double getElapsedTime() const { return m_elapsedTime; }

    /**
     * Get current FPS.
     */
    float getFPS() const { return m_fps; }

    /**
     * Get frame count.
     */
    uint64_t getFrameCount() const { return m_frameCount; }

    /**
     * Get the assets path.
     */
    const std::string& getAssetsPath() const { return m_assetsPath; }

private:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool initializeSubsystems();
    void shutdownSubsystems();
    void processFrame();

    EngineState m_state = EngineState::Uninitialized;
    bool m_quitRequested = false;

    // Timing
    float m_deltaTime = 0.0f;
    double m_elapsedTime = 0.0;
    float m_fps = 0.0f;
    uint64_t m_frameCount = 0;
    uint64_t m_lastTime = 0;

    // FPS calculation
    float m_fpsAccumulator = 0.0f;
    int m_fpsFrameCount = 0;

    // Paths
    std::string m_assetsPath;

    // Callbacks
    UpdateCallback m_updateCallback;
    RenderCallback m_renderCallback;
    EventCallback m_eventCallback;

    // Headless mode (no graphics)
    bool m_headless = false;
};

} // namespace mcgng

#endif // MCGNG_ENGINE_H
