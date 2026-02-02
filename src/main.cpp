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
#include "graphics/terrain.h"
#include "audio/audio_system.h"
#include "audio/music_manager.h"
#include "assets/pak_reader.h"
#include "assets/shape_reader.h"
#include "assets/nested_pak_reader.h"
#include "assets/fst_reader.h"
#include "assets/tga_loader.h"

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
std::unique_ptr<mcgng::Sprite> g_mechSprite;
std::shared_ptr<mcgng::TerrainTileset> g_tileset;
mcgng::Palette g_palette;
int g_currentFrame = 0;
float g_frameTimer = 0.0f;

// Mech sprite data
mcgng::NestedPakReader g_mechPak;

// Music
mcgng::MusicHandle g_musicTrack = mcgng::INVALID_MUSIC;

// UI textures
mcgng::TextureHandle g_uiButtonTexture = mcgng::INVALID_TEXTURE;
mcgng::TextureHandle g_uiBackgroundTexture = mcgng::INVALID_TEXTURE;

// Macro for logging to both console and file
#define LOG(msg) do { std::cout << msg << std::endl; if (g_debugLog.is_open()) g_debugLog << msg << std::endl; } while(0)

bool loadGamePalette(const std::string& assetsPath) {
    // Try to load palette from MISC.FST
    std::vector<std::string> fstPaths = {
        assetsPath + "\\MISC.FST",
        assetsPath + "/MISC.FST",
    };

    mcgng::FstReader fst;
    for (const auto& path : fstPaths) {
        if (fst.open(path)) {
            // Look for HB.PAL (main game palette)
            auto palData = fst.readFile("data/palette/HB.PAL");

            if (!palData.empty() && palData.size() >= 700) {
                // MCG palettes may use 6-bit values (0-63), need to scale to 8-bit
                bool is6bit = true;
                for (size_t i = 0; i < std::min(palData.size(), size_t(768)); ++i) {
                    if (palData[i] > 63) {
                        is6bit = false;
                        break;
                    }
                }

                if (g_palette.load(palData.data(), palData.size(), is6bit)) {
                    LOG("Loaded game palette from MISC.FST (HB.PAL, " +
                        std::string(is6bit ? "6-bit" : "8-bit") + ")");
                    return true;
                }
            }
            break;
        }
    }

    LOG("Could not load game palette, using default");
    g_palette = mcgng::Palette::createDefault();
    return false;
}

bool loadTestSprites(const std::string& assetsPath) {
    // Load game palette first
    loadGamePalette(assetsPath);

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

bool loadMechSprites(const std::string& assetsPath) {
    // Try to load mech torsos
    std::vector<std::string> mechPaths = {
        assetsPath + "\\DATA\\SPRITES\\TORSOS.PAK",
        assetsPath + "/DATA/SPRITES/TORSOS.PAK",
    };

    for (const auto& path : mechPaths) {
        LOG("Trying to open mech PAK: " + path);
        if (g_mechPak.open(path)) {
            LOG("Loaded mech PAK with " + std::to_string(g_mechPak.getMechCount()) + " mech types");

            // Try to create sprite from any mech with valid frames
            for (uint32_t m = 0; m < g_mechPak.getMechCount(); ++m) {
                const mcgng::MechSpriteSet* mech = g_mechPak.getMech(m);
                if (!mech) continue;

                // Try mech format first - look for a larger frame
                if (mech->getMechFrameCount() > 0) {
                    // Try to find a larger frame (later frames tend to be bigger)
                    int bestIdx = -1;
                    int bestSize = 0;
                    for (uint32_t f = 0; f < mech->getMechFrameCount(); ++f) {
                        const mcgng::MechShapeReader* fr = mech->getMechFrame(f);
                        if (fr && fr->isLoaded()) {
                            int sz = fr->getWidth() * fr->getHeight();
                            if (sz > bestSize) {
                                bestSize = sz;
                                bestIdx = static_cast<int>(f);
                            }
                        }
                    }
                    if (bestIdx >= 0) {
                        const mcgng::MechShapeReader* frame = mech->getMechFrame(static_cast<uint32_t>(bestIdx));
                        mcgng::ShapeData shapeData = frame->decode();
                        if (!shapeData.pixels.empty()) {
                            g_mechSprite = std::make_unique<mcgng::Sprite>();
                            if (g_mechSprite->loadFromShape(shapeData, g_palette)) {
                                LOG("Loaded mech sprite from type " + std::to_string(m) +
                                    ": " + std::to_string(shapeData.width) +
                                    "x" + std::to_string(shapeData.height));
                                return true;
                            }
                        }
                    }
                }

                // Fall back to standard shape format
                if (mech->getFrameCount() > 0) {
                    const mcgng::ShapeReader* frame = mech->getFrame(0);
                    if (frame && frame->getShapeCount() > 0) {
                        mcgng::ShapeData shapeData = frame->decodeShape(0);
                        if (!shapeData.pixels.empty()) {
                            g_mechSprite = std::make_unique<mcgng::Sprite>();
                            if (g_mechSprite->loadFromShape(shapeData, g_palette)) {
                                LOG("Loaded mech sprite (std) from type " + std::to_string(m) +
                                    ": " + std::to_string(shapeData.width) +
                                    "x" + std::to_string(shapeData.height));
                                return true;
                            }
                        }
                    }
                }
            }
            break;
        }
    }

    LOG("Failed to load mech sprites");
    return false;
}

bool loadTerrainTiles(const std::string& assetsPath) {
    // Try to load terrain tiles
    std::vector<std::string> tilePaths = {
        assetsPath + "\\DATA\\TILES\\TILES.PAK",
        assetsPath + "/DATA/TILES/TILES.PAK",
    };

    mcgng::PakReader pak;
    for (const auto& path : tilePaths) {
        if (pak.open(path)) {
            LOG("Opened tiles PAK: " + path + " with " + std::to_string(pak.getNumPackets()) + " packets");

            // TILES.PAK has null packets at the start, real tiles start around 4014
            // Let's find the first non-null packet
            g_tileset = std::make_shared<mcgng::TerrainTileset>();
            auto& renderer = mcgng::Renderer::instance();

            int tilesLoaded = 0;
            int checkedPackets = 0;
            int maxPacketsToCheck = 5;  // Keep small for fast startup

            int nullSkipped = 0;
            for (size_t i = 4014; i < pak.getNumPackets() && tilesLoaded < 5 && checkedPackets < maxPacketsToCheck; ++i) {
                auto entry = pak.getEntry(i);
                if (!entry || entry->storageType == mcgng::PakStorageType::NUL) {
                    ++nullSkipped;
                    continue;
                }

                std::vector<uint8_t> tileData = pak.readPacket(i);
                if (tileData.empty()) {
                    continue;
                }

                ++checkedPackets;

                // Try different tile sizes
                // Original MCG tiles might be smaller or use different formats
                int width = 0, height = 0;

                // MCG tiles are typically 45x45 or 90x90
                // The data might include headers or be RLE compressed
                if (tileData.size() >= 4050) {  // 90x45 or more
                    width = 90; height = 45;
                } else if (tileData.size() >= 2025) {  // 45x45
                    width = height = 45;
                } else if (tileData.size() >= 1024) {  // 32x32
                    width = height = 32;
                } else if (tileData.size() >= 400) {  // 20x20
                    width = height = 20;
                } else if (tileData.size() >= 256) {  // 16x16
                    width = height = 16;
                }

                if (width > 0) {
                    int idx = g_tileset->addTile(tileData.data(), g_palette.data(), width, height);
                    if (idx >= 0) {
                        ++tilesLoaded;
                    }
                }
            }

            LOG("Tiles: checked " + std::to_string(checkedPackets) + " packets, skipped " +
                std::to_string(nullSkipped) + " null, loaded " + std::to_string(tilesLoaded));
            return tilesLoaded > 0;
        }
    }

    LOG("Failed to load terrain tiles");
    return false;
}

bool initializeAudio(const std::string& assetsPath) {
    // Initialize audio system
    auto& audio = mcgng::AudioSystem::instance();
    if (!audio.initialize()) {
        LOG("Failed to initialize audio system");
        return false;
    }
    LOG("Audio system initialized");

    // Initialize music manager
    auto& music = mcgng::MusicManager::instance();
    if (!music.initialize()) {
        LOG("Failed to initialize music manager");
        return false;
    }
    LOG("Music manager initialized");

    // Try to load a music track
    std::vector<std::string> musicPaths = {
        assetsPath + "\\DATA\\SOUND\\MUSIC00.WAV",
        assetsPath + "/DATA/SOUND/MUSIC00.WAV",
    };

    for (const auto& path : musicPaths) {
        g_musicTrack = music.loadTrack(path);
        if (g_musicTrack != mcgng::INVALID_MUSIC) {
            LOG("Loaded music track: " + path);
            // Start playing with fade in
            music.play(g_musicTrack, 2.0f);  // 2 second fade in
            music.setVolume(0.5f);  // 50% volume
            LOG("Music playback started");
            return true;
        }
    }

    LOG("Could not load music track");
    return false;
}

bool loadUITextures(const std::string& assetsPath) {
    // Try to load UI graphics from extracted TGA files
    std::vector<std::string> tgaPaths = {
        assetsPath + "\\ART.FST\\BG_EXIT.tga",
        assetsPath + "/ART.FST/BG_EXIT.tga",
    };

    // Try the extracted MCGExtracted folder too
    std::string mcgExtracted = "D:\\mcgoldng\\mcgextracted\\MCGExtracted-master\\ART.FST";
    tgaPaths.push_back(mcgExtracted + "\\BG_EXIT.tga");
    tgaPaths.push_back(mcgExtracted + "\\ACCESS00.tga");

    auto& renderer = mcgng::Renderer::instance();

    for (const auto& path : tgaPaths) {
        mcgng::TgaImage img = mcgng::TgaLoader::loadFromFile(path);
        if (img.isValid()) {
            // Create texture from TGA
            g_uiButtonTexture = renderer.createTexture(img.pixels.data(), img.width, img.height);
            if (g_uiButtonTexture != mcgng::INVALID_TEXTURE) {
                LOG("Loaded UI texture: " + path + " (" + std::to_string(img.width) + "x" + std::to_string(img.height) + ")");
                return true;
            }
        }
    }

    LOG("Could not load UI textures");
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

    // Try to load test sprites (cursors)
    bool spritesLoaded = loadTestSprites(options.assetsPath);
    if (spritesLoaded) {
        std::cout << "Cursor sprites loaded successfully!\n";
    } else {
        std::cout << "Could not load cursor sprites\n";
    }

    // Try to load mech sprites
    bool mechsLoaded = loadMechSprites(options.assetsPath);
    if (mechsLoaded) {
        std::cout << "Mech sprites loaded successfully!\n";
    } else {
        std::cout << "Could not load mech sprites\n";
    }

    // Try to load terrain tiles
    bool terrainLoaded = loadTerrainTiles(options.assetsPath);
    if (terrainLoaded) {
        std::cout << "Terrain tiles loaded successfully!\n";
    } else {
        std::cout << "Could not load terrain tiles\n";
    }

    // Initialize audio and start music
    bool audioLoaded = initializeAudio(options.assetsPath);
    if (audioLoaded) {
        std::cout << "Audio initialized and music playing!\n";
    } else {
        std::cout << "Audio not available\n";
    }

    // Load UI textures
    bool uiLoaded = loadUITextures(options.assetsPath);
    if (uiLoaded) {
        std::cout << "UI textures loaded!\n";
    } else {
        std::cout << "Could not load UI textures\n";
    }

    // Set up callbacks
    engine.setUpdateCallback([](float deltaTime) {
        // Update music manager (for fade effects)
        mcgng::MusicManager::instance().update(deltaTime);

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

    // Debug: Check loading status
    LOG("Render check: mechSprite=" + std::string(g_mechSprite ? "exists" : "null") +
        " isLoaded=" + std::string((g_mechSprite && g_mechSprite->isLoaded()) ? "yes" : "no"));
    LOG("Render check: tileset=" + std::string(g_tileset ? "exists" : "null") +
        " count=" + std::to_string(g_tileset ? g_tileset->getTileCount() : 0));

    engine.setRenderCallback([]() {
        auto& renderer = mcgng::Renderer::instance();

        // Draw cursor sprites at top
        if (g_testSprite && g_testSprite->isLoaded()) {
            for (int i = 0; i < 8; ++i) {
                g_testSprite->draw(50 + i * 60, 50);
            }
        }

        // Draw mech sprite if loaded
        if (g_mechSprite && g_mechSprite->isLoaded()) {
            // Draw a bright marker behind the mech so we can see if position is correct
            renderer.setDrawColor({255, 0, 255, 255});  // Bright magenta
            renderer.drawRect({355, 255, 90, 90});  // Box behind scaled mech

            // Draw at center, scaled up
            g_mechSprite->drawScaled(400, 300, 3.0f, 3.0f);

            // Draw markers and smaller versions
            renderer.setDrawColor({0, 255, 0, 255});  // Bright green
            renderer.drawRect({95, 395, 30, 30});  // Box behind first small mech

            g_mechSprite->draw(100, 400);
            g_mechSprite->draw(200, 400);
            g_mechSprite->draw(300, 400);
        } else {
            // Draw placeholder rectangle so we can see something
            renderer.setDrawColor({100, 100, 150, 255});
            renderer.drawRect({350, 250, 100, 100});
        }

        // Draw terrain tiles if loaded (sample grid)
        if (g_tileset && g_tileset->getTileCount() > 0) {
            int tileX = 500;
            int tileY = 150;
            int tilesPerRow = 10;
            int tilesDrawn = 0;
            int maxTiles = std::min(50, g_tileset->getTileCount());

            for (int i = 0; i < maxTiles; ++i) {
                mcgng::TextureHandle tex = g_tileset->getTileTexture(i);
                if (tex != mcgng::INVALID_TEXTURE) {
                    int x = tileX + (tilesDrawn % tilesPerRow) * 50;
                    int y = tileY + (tilesDrawn / tilesPerRow) * 50;
                    renderer.drawTexture(tex, x, y);
                    ++tilesDrawn;
                }
            }
        }

        // Draw UI texture if loaded
        if (g_uiButtonTexture != mcgng::INVALID_TEXTURE) {
            renderer.drawTexture(g_uiButtonTexture, 600, 500);
        }

        // Draw info panel background
        renderer.setDrawColor({30, 30, 40, 220});
        renderer.drawRect({10, 10, 200, 30});
    });

    engine.setEventCallback([]() -> bool {
        // Event processing here
        // Return false to quit
        return true;
    });

    // Run main loop
    engine.run();

    // Cleanup audio
    mcgng::MusicManager::instance().shutdown();
    mcgng::AudioSystem::instance().shutdown();

    // Cleanup engine
    engine.shutdown();

    std::cout << "Thank you for playing!\n";

    if (g_debugLog.is_open()) {
        g_debugLog << "Shutting down normally" << std::endl;
        g_debugLog.close();
    }

    return 0;
}
