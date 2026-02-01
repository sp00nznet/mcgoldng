#include "core/engine.h"
#include "core/config.h"
#include <iostream>
#include <chrono>
#include <thread>

namespace mcgng {

Engine& Engine::instance() {
    static Engine instance;
    return instance;
}

Engine::Engine() = default;

Engine::~Engine() {
    if (m_state != EngineState::Terminated && m_state != EngineState::Uninitialized) {
        shutdown();
    }
}

bool Engine::initialize(const EngineOptions& options) {
    if (m_state != EngineState::Uninitialized) {
        std::cerr << "Engine: Already initialized\n";
        return false;
    }

    m_state = EngineState::Initializing;
    m_headless = options.headless;
    m_assetsPath = options.assetsPath;

    std::cout << "Engine: Initializing...\n";

    // Load configuration
    auto& config = ConfigManager::instance();
    if (!options.configPath.empty()) {
        config.load(options.configPath);
    }

    // Update config with provided paths
    if (!options.assetsPath.empty()) {
        config.get().assetsPath = options.assetsPath;
    }

    // Initialize subsystems
    if (!initializeSubsystems()) {
        std::cerr << "Engine: Failed to initialize subsystems\n";
        m_state = EngineState::Terminated;
        return false;
    }

    // Initialize timing
    auto now = std::chrono::high_resolution_clock::now();
    m_lastTime = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();

    m_state = EngineState::Running;
    std::cout << "Engine: Initialization complete\n";
    return true;
}

bool Engine::initializeSubsystems() {
    // In headless mode, we don't initialize graphics/audio
    if (m_headless) {
        std::cout << "Engine: Running in headless mode\n";
        return true;
    }

    // Initialize SDL, audio, video subsystems here when implemented
    // For now, just return success
    std::cout << "Engine: Subsystems initialized\n";
    return true;
}

void Engine::shutdown() {
    if (m_state == EngineState::Terminated || m_state == EngineState::Uninitialized) {
        return;
    }

    m_state = EngineState::ShuttingDown;
    std::cout << "Engine: Shutting down...\n";

    shutdownSubsystems();

    m_state = EngineState::Terminated;
    std::cout << "Engine: Shutdown complete\n";
}

void Engine::shutdownSubsystems() {
    // Shutdown SDL, audio, video subsystems here when implemented
}

void Engine::run() {
    if (m_state != EngineState::Running && m_state != EngineState::Paused) {
        std::cerr << "Engine: Cannot run - not initialized\n";
        return;
    }

    const auto& config = ConfigManager::instance().get();
    const float targetFrameTime = config.targetFPS > 0 ? 1.0f / config.targetFPS : 0.0f;

    std::cout << "Engine: Starting main loop\n";

    while (!m_quitRequested && (m_state == EngineState::Running || m_state == EngineState::Paused)) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        processFrame();

        // Frame rate limiting
        if (targetFrameTime > 0.0f) {
            auto frameEnd = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float>(frameEnd - frameStart).count();

            if (frameTime < targetFrameTime) {
                auto sleepTime = std::chrono::duration<float>(targetFrameTime - frameTime);
                std::this_thread::sleep_for(sleepTime);
            }
        }
    }

    std::cout << "Engine: Main loop ended\n";
}

void Engine::processFrame() {
    // Calculate delta time
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t currentTime = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();

    m_deltaTime = static_cast<float>(currentTime - m_lastTime) / 1000000.0f;
    m_lastTime = currentTime;

    // Clamp delta time to avoid huge jumps (e.g., after breakpoints)
    if (m_deltaTime > 0.1f) {
        m_deltaTime = 0.1f;
    }

    m_elapsedTime += m_deltaTime;
    ++m_frameCount;

    // Update FPS counter
    m_fpsAccumulator += m_deltaTime;
    ++m_fpsFrameCount;
    if (m_fpsAccumulator >= 1.0f) {
        m_fps = static_cast<float>(m_fpsFrameCount) / m_fpsAccumulator;
        m_fpsAccumulator = 0.0f;
        m_fpsFrameCount = 0;
    }

    // Process events
    if (m_eventCallback) {
        if (!m_eventCallback()) {
            m_quitRequested = true;
            return;
        }
    }

    // Update (skip if paused)
    if (m_state == EngineState::Running && m_updateCallback) {
        m_updateCallback(m_deltaTime);
    }

    // Render (always, even when paused)
    if (!m_headless && m_renderCallback) {
        m_renderCallback();
    }
}

void Engine::quit() {
    m_quitRequested = true;
}

void Engine::pause() {
    if (m_state == EngineState::Running) {
        m_state = EngineState::Paused;
        std::cout << "Engine: Paused\n";
    }
}

void Engine::resume() {
    if (m_state == EngineState::Paused) {
        m_state = EngineState::Running;
        std::cout << "Engine: Resumed\n";
    }
}

} // namespace mcgng
