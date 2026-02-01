#ifndef MCGNG_PALETTE_H
#define MCGNG_PALETTE_H

#include <cstdint>
#include <vector>
#include <string>
#include <array>

namespace mcgng {

/**
 * 256-color palette for indexed graphics.
 *
 * MCG uses several palette formats:
 * - .PAL files: 768 bytes (256 * 3 RGB values)
 * - Some palettes have 6-bit values (0-63), need scaling to 8-bit
 */
class Palette {
public:
    static constexpr int NUM_COLORS = 256;
    static constexpr int BYTES_PER_COLOR = 3;  // RGB

    Palette() { clear(); }

    /**
     * Load palette from raw data.
     * @param data Pointer to palette data (768 bytes RGB)
     * @param size Size of data
     * @param is6bit True if values are 6-bit (0-63), will scale to 8-bit
     * @return true on success
     */
    bool load(const uint8_t* data, size_t size, bool is6bit = false);

    /**
     * Load palette from file.
     * @param path Path to .PAL file
     * @return true on success
     */
    bool loadFromFile(const std::string& path);

    /**
     * Clear palette to black.
     */
    void clear();

    /**
     * Get raw palette data pointer (768 bytes).
     */
    const uint8_t* data() const { return m_colors.data(); }

    /**
     * Get color at index.
     * @param index Color index (0-255)
     * @return Pointer to RGB triplet, or nullptr if invalid
     */
    const uint8_t* getColor(uint8_t index) const {
        return &m_colors[index * BYTES_PER_COLOR];
    }

    /**
     * Get color components.
     */
    uint8_t getRed(uint8_t index) const { return m_colors[index * 3]; }
    uint8_t getGreen(uint8_t index) const { return m_colors[index * 3 + 1]; }
    uint8_t getBlue(uint8_t index) const { return m_colors[index * 3 + 2]; }

    /**
     * Set a specific color.
     */
    void setColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

    /**
     * Convert indexed pixels to RGBA.
     * @param indexed Source indexed pixel data
     * @param rgba Destination RGBA buffer (must be 4x size of indexed)
     * @param pixelCount Number of pixels
     * @param transparentIndex Index to treat as transparent (default 0)
     */
    void convertToRGBA(const uint8_t* indexed, uint8_t* rgba,
                       size_t pixelCount, uint8_t transparentIndex = 0) const;

    /**
     * Convert indexed pixels to RGBA with full alpha.
     * (No transparent index, all pixels opaque)
     */
    void convertToRGBAOpaque(const uint8_t* indexed, uint8_t* rgba,
                              size_t pixelCount) const;

    /**
     * Check if loaded/valid.
     */
    bool isValid() const { return m_valid; }

    /**
     * Create a grayscale palette.
     */
    static Palette createGrayscale();

    /**
     * Create default MCG-style palette (for testing).
     */
    static Palette createDefault();

private:
    std::array<uint8_t, NUM_COLORS * BYTES_PER_COLOR> m_colors;
    bool m_valid = false;
};

/**
 * Palette manager for loading and caching game palettes.
 */
class PaletteManager {
public:
    static PaletteManager& instance();

    /**
     * Load a named palette.
     * @param name Palette identifier (e.g., "HB", "MENU")
     * @param path Full path to palette file
     * @return true on success
     */
    bool loadPalette(const std::string& name, const std::string& path);

    /**
     * Get a loaded palette.
     * @param name Palette identifier
     * @return Pointer to palette, or nullptr if not found
     */
    const Palette* getPalette(const std::string& name) const;

    /**
     * Get the default game palette.
     */
    const Palette* getDefaultPalette() const;

    /**
     * Set the default palette name.
     */
    void setDefaultPalette(const std::string& name);

    /**
     * Clear all loaded palettes.
     */
    void clear();

private:
    PaletteManager() = default;

    std::vector<std::pair<std::string, Palette>> m_palettes;
    std::string m_defaultPalette;
};

} // namespace mcgng

#endif // MCGNG_PALETTE_H
