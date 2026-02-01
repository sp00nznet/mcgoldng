/**
 * MCG-Extract: Asset Extraction Tool for MechCommander Gold
 *
 * Usage: mcg-extract <iso-path-or-game-folder> <output-dir>
 *
 * This tool extracts assets from:
 * - FST archives (ART.FST, MISSION.FST, MISC.FST, SHAPES.FST, TERRAIN.FST)
 * - PAK archives (various .PAK files)
 *
 * Part of the MechCommander Gold: Next Generation project.
 */

#include "assets/fst_reader.h"
#include "assets/pak_reader.h"
#include "assets/fit_parser.h"

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>

namespace fs = std::filesystem;
using namespace mcgng;

// List of FST archives to extract
static const std::vector<std::string> FST_ARCHIVES = {
    "ART.FST",
    "MISSION.FST",
    "MISC.FST",
    "SHAPES.FST",
    "TERRAIN.FST"
};

// List of known PAK directories/patterns
static const std::vector<std::string> PAK_DIRECTORIES = {
    "SPRITES",
    "TILES",
    "SOUND",
    "MOVIES"
};

void printUsage(const char* programName) {
    std::cout << "MCG-Extract: Asset Extraction Tool for MechCommander Gold\n\n";
    std::cout << "Usage: " << programName << " <game-folder> <output-dir>\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  game-folder  Path to MechCommander Gold installation or mounted ISO\n";
    std::cout << "  output-dir   Directory to extract assets to\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " D:\\Games\\MCG extracted_assets\n";
    std::cout << "  " << programName << " E:\\ output\n\n";
    std::cout << "This tool will extract:\n";
    std::cout << "  - FST archives: ART.FST, MISSION.FST, MISC.FST, SHAPES.FST, TERRAIN.FST\n";
    std::cout << "  - PAK archives: SPRITES/*.PAK, TILES/*.PAK, etc.\n";
}

void printProgress(float progress, const std::string& filename) {
    int barWidth = 40;
    int pos = static_cast<int>(barWidth * progress);

    std::cout << "\r[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << std::setw(3) << static_cast<int>(progress * 100) << "% ";

    // Truncate filename if too long
    if (filename.length() > 30) {
        std::cout << "..." << filename.substr(filename.length() - 27);
    } else {
        std::cout << filename;
    }

    // Clear rest of line
    std::cout << "                    " << std::flush;
}

fs::path findGameDataPath(const fs::path& basePath) {
    // Try common data folder locations
    std::vector<fs::path> candidates = {
        basePath / "DATA",
        basePath / "data",
        basePath / "Data",
        basePath / "GAMEDATA",
        basePath / "GameData",
        basePath
    };

    for (const auto& candidate : candidates) {
        // Look for a signature file (any .FST file)
        if (fs::exists(candidate)) {
            for (const auto& fst : FST_ARCHIVES) {
                if (fs::exists(candidate / fst)) {
                    return candidate;
                }
            }
        }
    }

    return basePath;
}

size_t extractFstArchives(const fs::path& dataPath, const fs::path& outputDir) {
    size_t totalExtracted = 0;

    for (const auto& fstName : FST_ARCHIVES) {
        fs::path fstPath = dataPath / fstName;

        if (!fs::exists(fstPath)) {
            std::cout << "  [SKIP] " << fstName << " not found\n";
            continue;
        }

        std::cout << "\n  Extracting " << fstName << "...\n";

        FstReader reader;
        if (!reader.open(fstPath.string())) {
            std::cout << "  [ERROR] Failed to open " << fstName << "\n";
            continue;
        }

        std::cout << "    Found " << reader.getNumFiles() << " files\n";

        // Create subdirectory for this FST
        fs::path fstOutputDir = outputDir / fstName;
        std::error_code ec;
        fs::create_directories(fstOutputDir, ec);

        size_t extracted = reader.extractAll(fstOutputDir.string(),
            [](float progress, const std::string& filename) {
                printProgress(progress, filename);
            });

        std::cout << "\n    Extracted " << extracted << " files\n";
        totalExtracted += extracted;
    }

    return totalExtracted;
}

size_t extractPakFiles(const fs::path& dataPath, const fs::path& outputDir) {
    size_t totalExtracted = 0;

    // Find all .PAK files recursively
    std::vector<fs::path> pakFiles;

    for (const auto& entry : fs::recursive_directory_iterator(dataPath)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                          [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            if (ext == ".PAK") {
                pakFiles.push_back(entry.path());
            }
        }
    }

    if (pakFiles.empty()) {
        std::cout << "  No PAK files found\n";
        return 0;
    }

    std::cout << "  Found " << pakFiles.size() << " PAK files\n";

    for (const auto& pakPath : pakFiles) {
        // Calculate relative path for output
        fs::path relativePath = fs::relative(pakPath, dataPath);
        fs::path pakOutputDir = outputDir / "PAK" / relativePath.parent_path() / relativePath.stem();

        std::cout << "\n  Extracting " << relativePath << "...\n";

        PakReader reader;
        if (!reader.open(pakPath.string())) {
            std::cout << "  [ERROR] Failed to open\n";
            continue;
        }

        std::cout << "    Found " << reader.getNumPackets() << " packets\n";

        std::error_code ec;
        fs::create_directories(pakOutputDir, ec);

        size_t extracted = reader.extractAll(pakOutputDir.string(), "pkt_",
            [](float progress, size_t index) {
                std::cout << "\r    Progress: " << std::setw(3)
                          << static_cast<int>(progress * 100) << "% "
                          << "(packet " << index << ")" << std::flush;
            });

        std::cout << "\n    Extracted " << extracted << " packets\n";
        totalExtracted += extracted;
    }

    return totalExtracted;
}

void listContents(const fs::path& dataPath) {
    std::cout << "\nArchive Contents Summary:\n";
    std::cout << std::string(60, '-') << "\n";

    for (const auto& fstName : FST_ARCHIVES) {
        fs::path fstPath = dataPath / fstName;

        if (!fs::exists(fstPath)) {
            continue;
        }

        FstReader reader;
        if (reader.open(fstPath.string())) {
            std::cout << fstName << ": " << reader.getNumFiles() << " files\n";

            // Show first few entries
            const auto& entries = reader.getEntries();
            size_t showCount = std::min<size_t>(3, entries.size());
            for (size_t i = 0; i < showCount; ++i) {
                std::cout << "    " << entries[i].filePath;
                if (entries[i].isCompressed()) {
                    std::cout << " [compressed: " << entries[i].compressedSize
                              << " -> " << entries[i].uncompressedSize << "]";
                }
                std::cout << "\n";
            }
            if (entries.size() > showCount) {
                std::cout << "    ... and " << (entries.size() - showCount) << " more\n";
            }
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "MCG-Extract v0.1.0\n";
    std::cout << "MechCommander Gold Asset Extractor\n";
    std::cout << "========================================\n\n";

    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    fs::path gamePath = argv[1];
    fs::path outputDir = argv[2];

    // Validate input path
    if (!fs::exists(gamePath)) {
        std::cerr << "Error: Game path does not exist: " << gamePath << "\n";
        return 1;
    }

    // Find the DATA folder
    fs::path dataPath = findGameDataPath(gamePath);
    std::cout << "Game data path: " << dataPath << "\n";

    // Create output directory
    std::error_code ec;
    fs::create_directories(outputDir, ec);
    if (ec) {
        std::cerr << "Error: Failed to create output directory: " << outputDir << "\n";
        return 1;
    }

    std::cout << "Output directory: " << outputDir << "\n\n";

    // Show contents summary
    listContents(dataPath);

    // Start extraction
    auto startTime = std::chrono::steady_clock::now();

    std::cout << "\n========================================\n";
    std::cout << "Extracting FST Archives...\n";
    std::cout << "========================================\n";

    size_t fstExtracted = extractFstArchives(dataPath, outputDir);

    std::cout << "\n========================================\n";
    std::cout << "Extracting PAK Archives...\n";
    std::cout << "========================================\n";

    size_t pakExtracted = extractPakFiles(dataPath, outputDir);

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);

    std::cout << "\n========================================\n";
    std::cout << "Extraction Complete!\n";
    std::cout << "========================================\n";
    std::cout << "  FST files extracted: " << fstExtracted << "\n";
    std::cout << "  PAK packets extracted: " << pakExtracted << "\n";
    std::cout << "  Total time: " << duration.count() << " seconds\n";
    std::cout << "  Output: " << outputDir << "\n";

    return 0;
}
