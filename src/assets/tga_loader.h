#ifndef MCGNG_TGA_LOADER_H
#define MCGNG_TGA_LOADER_H

#include <cstdint>
#include <vector>
#include <string>

namespace mcgng {

/**
 * TGA image data.
 */
struct TgaImage {
    int width = 0;
    int height = 0;
    int bitsPerPixel = 0;       // 8, 16, 24, or 32
    bool hasAlpha = false;
    std::vector<uint8_t> pixels; // RGBA format (always converted)

    bool isValid() const { return width > 0 && height > 0 && !pixels.empty(); }
};

/**
 * TGA file loader.
 *
 * Supports:
 * - Uncompressed true-color (type 2)
 * - RLE compressed true-color (type 10)
 * - Uncompressed grayscale (type 3)
 * - RLE compressed grayscale (type 11)
 * - Color-mapped images (type 1, 9)
 */
class TgaLoader {
public:
    /**
     * Load TGA from file.
     * @param path Path to TGA file
     * @return TgaImage with RGBA pixel data
     */
    static TgaImage loadFromFile(const std::string& path);

    /**
     * Load TGA from memory.
     * @param data TGA file data
     * @param size Size of data
     * @return TgaImage with RGBA pixel data
     */
    static TgaImage loadFromMemory(const uint8_t* data, size_t size);
};

} // namespace mcgng

#endif // MCGNG_TGA_LOADER_H
