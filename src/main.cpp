/**
 * MechCommander Gold: Next Generation
 *
 * A modern reimplementation of MechCommander Gold (1998) for Windows 11+.
 * Users provide their legal copy of the game; MCG-NG extracts assets
 * and runs them in a modern engine.
 */

#include "core/engine.h"
#include "core/config.h"
#include "graphics/renderer.h"
#include "graphics/sprite.h"
#include "graphics/palette.h"
#include "assets/pak_reader.h"
#include "assets/shape_reader.h"

#include <iostream>
#include <fstream>
#include <string>
#include <memory>

// Debug log file
std::ofstream g_debugLog;

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

// Global sprite for testing
std::unique_ptr<mcgng::Sprite> g_testSprite;
mcgng::Palette g_palette;
int g_currentFrame = 0;
float g_frameTimer = 0.0f;

// Macro for logging to both console and file
#define LOG(msg) do { std::cout << msg << std::endl; if (g_debugLog.is_open()) g_debugLog << msg << std::endl; } while(0)

bool loadTestSprites(const std::string& assetsPath) {
    // Create default palette for testing
    g_palette = mcgng::Palette::createDefault();

    LOG("loadTestSprites: assetsPath = " + assetsPath);

    // Try to load sprite PAKs from the game's DATA/SPRITES directory
    // CURSORS.PAK has simple shape tables, mech PAKs have nested PAK structure
    std::vector<std::string> pakPaths = {
        assetsPath + "\\DATA\\SPRITES\\CURSORS.PAK",
        assetsPath + "/DATA/SPRITES/CURSORS.PAK",
        assetsPath + "\\DATA\\SPRITES\\BLIP.PAK",
        assetsPath + "/DATA/SPRITES/BLIP.PAK",
    };

    mcgng::PakReader pak;
    bool pakOpened = false;

    for (const auto& path : pakPaths) {
        LOG("Trying to open: " + path);
        if (pak.open(path)) {
            LOG("Opened PAK: " + path);
            pakOpened = true;
            break;
        }
    }

    if (!pakOpened) {
        LOG("Failed to open any sprite PAK file");
        return false;
    }

    LOG("PAK contains " + std::to_string(pak.getNumPackets()) + " packets");

    // In CURSORS.PAK, each packet is a separate cursor shape table
    // Let's load multiple packets as frames for our sprite
    g_testSprite = std::make_unique<mcgng::Sprite>();
    std::vector<mcgng::SpriteFrame> frames;
    auto& renderer = mcgng::Renderer::instance();

    size_t maxPackets = std::min(pak.getNumPackets(), size_t(50));  // Limit to 50
    for (size_t i = 0; i < maxPackets; ++i) {
        std::vector<uint8_t> packetData = pak.readPacket(i);
        if (packetData.empty() || packetData.size() < 8) {
            continue;
        }

        // Check if this packet is a nested PAK (starts with 0xFEEDFACE)
        // Mech sprite PAKs use this nested structure - skip for now
        uint32_t magic = *reinterpret_cast<const uint32_t*>(packetData.data());
        if (magic == mcgng::PakReader::PAK_MAGIC) {
            LOG("Packet " + std::to_string(i) + ": Nested PAK (skipped)");
            continue;
        }

        // Try to load as direct shape table
        mcgng::ShapeReader shapes;
        if (!shapes.load(packetData.data(), packetData.size())) {
            continue;
        }

        // Get the first shape from this table
        if (shapes.getShapeCount() > 0) {
            mcgng::ShapeData shapeData = shapes.decodeShape(0);
            if (shapeData.pixels.empty() || shapeData.width <= 0 || shapeData.height <= 0) {
                continue;
            }

            // Convert to RGBA
            size_t pixelCount = static_cast<size_t>(shapeData.width * shapeData.height);
            std::vector<uint8_t> rgba(pixelCount * 4);
            g_palette.convertToRGBA(shapeData.pixels.data(), rgba.data(), pixelCount, 0);

            // Create texture
            mcgng::TextureHandle tex = renderer.createTexture(rgba.data(), shapeData.width, shapeData.height);
            if (tex != mcgng::INVALID_TEXTURE) {
                mcgng::SpriteFrame frame;
                frame.texture = tex;
                frame.width = shapeData.width;
                frame.height = shapeData.height;
                frame.offsetX = shapeData.hotspotX;
                frame.offsetY = shapeData.hotspotY;
                frames.push_back(frame);
            }
        }
    }

    if (!frames.empty()) {
        LOG("Loaded " + std::to_string(frames.size()) + " cursor frames from PAK");
        g_testSprite->loadFrames(std::move(frames));
        return true;
    }

    LOG("No valid shape tables found in PAK");
    return false;
}

int main(int argc, char* argv[]) {
    // Open debug log file
    g_debugLog.open("mcgoldng_debug.log");
    if (g_debugLog.is_open()) {
        g_debugLog << "MCG-NG Debug Log" << std::endl;
        g_debugLog << "=================" << std::endl;
    }

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

    // Try to load test sprites
    bool spritesLoaded = loadTestSprites(options.assetsPath);
    if (spritesLoaded) {
        std::cout << "Test sprites loaded successfully!\n";
    } else {
        std::cout << "Could not load test sprites - continuing with basic rendering\n";
    }

    // Set up callbacks
    engine.setUpdateCallback([](float deltaTime) {
        // Animate the test sprite
        if (g_testSprite && g_testSprite->getFrameCount() > 1) {
            g_frameTimer += deltaTime;
            if (g_frameTimer >= 0.1f) {  // 10 FPS animation
                g_frameTimer = 0.0f;
                g_currentFrame = (g_currentFrame + 1) % g_testSprite->getFrameCount();
                g_testSprite->setFrame(g_currentFrame);
            }
        }
    });

    engine.setRenderCallback([]() {
        auto& renderer = mcgng::Renderer::instance();

        // Draw the test sprite if loaded
        if (g_testSprite && g_testSprite->isLoaded()) {
            // Draw at multiple positions to show the sprite
            g_testSprite->draw(100, 100);
            g_testSprite->draw(200, 100);
            g_testSprite->draw(300, 100);
            g_testSprite->draw(400, 100);

            // Draw scaled version
            g_testSprite->drawScaled(100, 250, 2.0f, 2.0f);
            g_testSprite->drawScaled(250, 250, 3.0f, 3.0f);
        } else {
            // Draw placeholder text area
            renderer.setDrawColor({100, 100, 150, 255});
            renderer.drawRect({100, 100, 200, 50});
        }

        // Draw info text area
        renderer.setDrawColor({40, 40, 60, 200});
        renderer.drawRect({10, 550, 300, 40});
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

    if (g_debugLog.is_open()) {
        g_debugLog << "Shutting down normally" << std::endl;
        g_debugLog.close();
    }

    return 0;
}
