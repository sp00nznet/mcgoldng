#include "graphics/palette.h"
#include <fstream>
#include <iostream>
#include <cstring>

namespace mcgng {

bool Palette::load(const uint8_t* data, size_t size, bool is6bit) {
    // Standard palette is 768 bytes (256 * 3)
    if (!data || size < NUM_COLORS * BYTES_PER_COLOR) {
        return false;
    }

    if (is6bit) {
        // Scale 6-bit values (0-63) to 8-bit (0-255)
        for (int i = 0; i < NUM_COLORS * BYTES_PER_COLOR; ++i) {
            // Multiply by 4 and add original/4 for better distribution
            uint8_t val = data[i];
            m_colors[i] = static_cast<uint8_t>((val << 2) | (val >> 4));
        }
    } else {
        std::memcpy(m_colors.data(), data, NUM_COLORS * BYTES_PER_COLOR);
    }

    m_valid = true;
    return true;
}

bool Palette::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Palette: Failed to open file: " << path << std::endl;
        return false;
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    if (fileSize < NUM_COLORS * BYTES_PER_COLOR) {
        std::cerr << "Palette: File too small: " << path << " (" << fileSize << " bytes)" << std::endl;
        return false;
    }

    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);

    // Detect if 6-bit palette by checking max values
    // If all values are <= 63, it's likely 6-bit
    bool is6bit = true;
    for (size_t i = 0; i < NUM_COLORS * BYTES_PER_COLOR && i < fileSize; ++i) {
        if (data[i] > 63) {
            is6bit = false;
            break;
        }
    }

    return load(data.data(), fileSize, is6bit);
}

void Palette::clear() {
    m_colors.fill(0);
    m_valid = false;
}

void Palette::setColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    m_colors[index * 3] = r;
    m_colors[index * 3 + 1] = g;
    m_colors[index * 3 + 2] = b;
    m_valid = true;
}

void Palette::convertToRGBA(const uint8_t* indexed, uint8_t* rgba,
                            size_t pixelCount, uint8_t transparentIndex) const {
    for (size_t i = 0; i < pixelCount; ++i) {
        uint8_t idx = indexed[i];
        const uint8_t* color = getColor(idx);

        rgba[i * 4 + 0] = color[0];  // R
        rgba[i * 4 + 1] = color[1];  // G
        rgba[i * 4 + 2] = color[2];  // B
        rgba[i * 4 + 3] = (idx == transparentIndex) ? 0 : 255;  // A
    }
}

void Palette::convertToRGBAOpaque(const uint8_t* indexed, uint8_t* rgba,
                                   size_t pixelCount) const {
    for (size_t i = 0; i < pixelCount; ++i) {
        uint8_t idx = indexed[i];
        const uint8_t* color = getColor(idx);

        rgba[i * 4 + 0] = color[0];  // R
        rgba[i * 4 + 1] = color[1];  // G
        rgba[i * 4 + 2] = color[2];  // B
        rgba[i * 4 + 3] = 255;       // A
    }
}

Palette Palette::createGrayscale() {
    Palette pal;
    for (int i = 0; i < NUM_COLORS; ++i) {
        uint8_t v = static_cast<uint8_t>(i);
        pal.setColor(static_cast<uint8_t>(i), v, v, v);
    }
    return pal;
}

Palette Palette::createDefault() {
    // Create a basic color palette for testing
    // Similar to VGA default palette
    Palette pal;

    // First 16 colors: classic VGA colors
    static const uint8_t vgaColors[16][3] = {
        {0, 0, 0},       // 0: Black
        {0, 0, 170},     // 1: Blue
        {0, 170, 0},     // 2: Green
        {0, 170, 170},   // 3: Cyan
        {170, 0, 0},     // 4: Red
        {170, 0, 170},   // 5: Magenta
        {170, 85, 0},    // 6: Brown
        {170, 170, 170}, // 7: Light Gray
        {85, 85, 85},    // 8: Dark Gray
        {85, 85, 255},   // 9: Light Blue
        {85, 255, 85},   // 10: Light Green
        {85, 255, 255},  // 11: Light Cyan
        {255, 85, 85},   // 12: Light Red
        {255, 85, 255},  // 13: Light Magenta
        {255, 255, 85},  // 14: Yellow
        {255, 255, 255}  // 15: White
    };

    for (int i = 0; i < 16; ++i) {
        pal.setColor(static_cast<uint8_t>(i), vgaColors[i][0], vgaColors[i][1], vgaColors[i][2]);
    }

    // 16-31: grayscale ramp
    for (int i = 16; i < 32; ++i) {
        uint8_t v = static_cast<uint8_t>((i - 16) * 17);
        pal.setColor(static_cast<uint8_t>(i), v, v, v);
    }

    // 32-255: color cube (6x6x6) + remaining grayscale
    int idx = 32;
    for (int r = 0; r < 6 && idx < 256; ++r) {
        for (int g = 0; g < 6 && idx < 256; ++g) {
            for (int b = 0; b < 6 && idx < 256; ++b) {
                pal.setColor(static_cast<uint8_t>(idx),
                             static_cast<uint8_t>(r * 51),
                             static_cast<uint8_t>(g * 51),
                             static_cast<uint8_t>(b * 51));
                ++idx;
            }
        }
    }

    // Fill remaining with grayscale
    while (idx < 256) {
        uint8_t v = static_cast<uint8_t>((idx - 232) * 10 + 8);
        pal.setColor(static_cast<uint8_t>(idx), v, v, v);
        ++idx;
    }

    return pal;
}

// PaletteManager implementation

PaletteManager& PaletteManager::instance() {
    static PaletteManager instance;
    return instance;
}

bool PaletteManager::loadPalette(const std::string& name, const std::string& path) {
    // Check if already loaded
    for (auto& pair : m_palettes) {
        if (pair.first == name) {
            return pair.second.loadFromFile(path);
        }
    }

    // Add new palette
    m_palettes.emplace_back();
    m_palettes.back().first = name;
    if (!m_palettes.back().second.loadFromFile(path)) {
        m_palettes.pop_back();
        return false;
    }

    // Set as default if first palette loaded
    if (m_defaultPalette.empty()) {
        m_defaultPalette = name;
    }

    std::cout << "PaletteManager: Loaded palette '" << name << "' from " << path << std::endl;
    return true;
}

const Palette* PaletteManager::getPalette(const std::string& name) const {
    for (const auto& pair : m_palettes) {
        if (pair.first == name) {
            return &pair.second;
        }
    }
    return nullptr;
}

const Palette* PaletteManager::getDefaultPalette() const {
    return getPalette(m_defaultPalette);
}

void PaletteManager::setDefaultPalette(const std::string& name) {
    m_defaultPalette = name;
}

void PaletteManager::clear() {
    m_palettes.clear();
    m_defaultPalette.clear();
}

} // namespace mcgng
