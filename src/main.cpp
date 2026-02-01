/**
 * MechCommander Gold: Next Generation
 *
 * A modern reimplementation of MechCommander Gold (1998) for Windows 11+.
 * Users provide their legal copy of the game; MCG-NG extracts assets
 * and runs them in a modern engine.
 */

#include "core/engine.h"
#include "core/config.h"

#include <iostream>
#include <string>

void printBanner() {
    std::cout << R"(
  __  __  ____ ____       _   _  ____
 |  \/  |/ ___|  _ \ ___ | \ | |/ ___|
 | |\/| | |   | |_) |___ |  \| | |  _
 | |  | | |___|  _ < ___ | |\  | |_| |
 |_|  |_|\____|_| \_\___ |_| \_|\____|

 MechCommander Gold: Next Generation
 Version 0.1.0

)" << std::endl;
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --assets <path>    Path to extracted game assets\n";
    std::cout << "  --config <path>    Path to configuration file\n";
    std::cout << "  --windowed         Start in windowed mode\n";
    std::cout << "  --fullscreen       Start in fullscreen mode\n";
    std::cout << "  --width <n>        Window width\n";
    std::cout << "  --height <n>       Window height\n";
    std::cout << "  --help             Show this help message\n";
}

int main(int argc, char* argv[]) {
    printBanner();

    mcgng::EngineOptions options;
    bool showHelp = false;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            showHelp = true;
        } else if (arg == "--assets" && i + 1 < argc) {
            options.assetsPath = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            options.configPath = argv[++i];
        } else if (arg == "--windowed") {
            mcgng::ConfigManager::instance().get().fullscreen = false;
        } else if (arg == "--fullscreen") {
            mcgng::ConfigManager::instance().get().fullscreen = true;
        } else if (arg == "--width" && i + 1 < argc) {
            mcgng::ConfigManager::instance().get().windowWidth = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            mcgng::ConfigManager::instance().get().windowHeight = std::stoi(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            showHelp = true;
        }
    }

    if (showHelp) {
        printUsage(argv[0]);
        return 0;
    }

    // Check for required assets path
    if (options.assetsPath.empty()) {
        auto& config = mcgng::ConfigManager::instance().get();
        if (config.assetsPath.empty()) {
            std::cerr << "Error: No assets path specified.\n";
            std::cerr << "Use --assets <path> to specify the extracted game assets.\n";
            std::cerr << "\nFirst, extract the game assets using:\n";
            std::cerr << "  mcg-extract <game-folder> <output-folder>\n";
            return 1;
        }
        options.assetsPath = config.assetsPath;
    }

    // Initialize engine
    auto& engine = mcgng::Engine::instance();
    if (!engine.initialize(options)) {
        std::cerr << "Failed to initialize engine\n";
        return 1;
    }

    // Set up callbacks
    engine.setUpdateCallback([](float deltaTime) {
        // Game update logic here
        (void)deltaTime;
    });

    engine.setRenderCallback([]() {
        // Rendering here
    });

    engine.setEventCallback([]() -> bool {
        // Event processing here
        // Return false to quit
        return true;
    });

    // Run main loop
    engine.run();

    // Cleanup
    engine.shutdown();

    std::cout << "Thank you for playing!\n";
    return 0;
}
