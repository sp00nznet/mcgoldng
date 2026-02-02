#ifndef MCGNG_SHAPE_READER_H
#define MCGNG_SHAPE_READER_H

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace mcgng {

/**
 * VFX Shape header (per shape within a table).
 */
struct ShapeHeader {
    int32_t bounds;     // Bounding info packed
    int32_t origin;     // Origin/hotspot packed
    int32_t xmin;       // Left edge
    int32_t ymin;       // Top edge
    int32_t xmax;       // Right edge
    int32_t ymax;       // Bottom edge
};

/**
 * Decoded shape data.
 */
struct ShapeData {
    int width = 0;
    int height = 0;
    int hotspotX = 0;   // Origin X (relative to xmin)
    int hotspotY = 0;   // Origin Y (relative to ymin)
    std::vector<uint8_t> pixels;  // 8-bit indexed pixel data

    // Get pixel at x,y (0 = transparent)
    uint8_t getPixel(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return 0;
        return pixels[y * width + x];
    }
};

/**
 * VFX Shape Table Reader.
 *
 * Shape table format:
 *   Bytes 0-3:   Version string (e.g., "1.10")
 *   Bytes 4-7:   Shape count
 *   Bytes 8+:    Offset table (8 bytes per shape)
 *
 * Each shape entry in offset table:
 *   Bytes 0-3:   Offset to shape data from table start
 *   Bytes 4-7:   (reserved/flags)
 *
 * Shape data:
 *   ShapeHeader (24 bytes)
 *   RLE pixel data
 */
class ShapeReader {
public:
    ShapeReader() = default;

    /**
     * Load shape table from memory.
     * @param data Pointer to shape table data
     * @param size Size of data in bytes
     * @return true on success
     */
    bool load(const uint8_t* data, size_t size);

    /**
     * Check if loaded.
     */
    bool isLoaded() const { return m_loaded; }

    /**
     * Get version string.
     */
    const std::string& getVersion() const { return m_version; }

    /**
     * Get number of shapes in table.
     */
    uint32_t getShapeCount() const { return m_shapeCount; }

    /**
     * Decode a specific shape.
     * @param index Shape index (0-based)
     * @return Decoded shape data, or empty on error
     */
    ShapeData decodeShape(uint32_t index) const;

    /**
     * Get shape header without decoding pixels.
     */
    bool getShapeHeader(uint32_t index, ShapeHeader& header) const;

    /**
     * Get raw shape data pointer and size.
     */
    bool getRawShape(uint32_t index, const uint8_t** data, size_t* size) const;

private:
    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
    bool m_loaded = false;
    size_t m_headerOffset = 0;  // Offset to version string (0 or 7)

    std::string m_version;
    uint32_t m_shapeCount = 0;
    std::vector<uint32_t> m_offsets;  // Offset to each shape

    // Decode RLE pixel data
    bool decodeRLE(const uint8_t* src, size_t srcSize,
                   uint8_t* dest, int width, int height) const;
};

/**
 * Mech Shape Reader - simplified format for mech sprites.
 *
 * MCG mech sprites have a 6-byte prefix before standard SHP format:
 *   Bytes 0-1:  Format ID (0x0100)
 *   Bytes 2-3:  Width (big-endian)
 *   Bytes 4-5:  Height (big-endian)
 *   Byte 6+:    Standard shape data or version string
 *
 * According to thegameengine.org: "Delete the first 6 bytes" for standard editors
 */
class MechShapeReader {
public:
    MechShapeReader() = default;

    /**
     * Load mech shape from memory.
     * @param data Pointer to shape data
     * @param size Size of data in bytes
     * @return true on success
     */
    bool load(const uint8_t* data, size_t size);

    /**
     * Check if loaded.
     */
    bool isLoaded() const { return m_loaded; }

    /**
     * Get dimensions.
     */
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

    /**
     * Decode the shape to pixel data.
     */
    ShapeData decode() const;

private:
    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
    bool m_loaded = false;
    int m_width = 0;
    int m_height = 0;
    size_t m_headerOffset = 6;  // Offset to shape data (after 6-byte prefix)
    std::string m_version;
};

/**
 * Load shapes from a PAK file packet.
 * Many MCG sprite PAK files contain shape tables in their packets.
 */
class ShapePackReader {
public:
    /**
     * Load shapes from a PAK file.
     * @param pakPath Path to PAK file
     * @return true on success
     */
    bool loadFromPak(const std::string& pakPath);

    /**
     * Get number of shape tables (packets) in the PAK.
     */
    uint32_t getTableCount() const { return static_cast<uint32_t>(m_tables.size()); }

    /**
     * Get a shape reader for a specific table.
     */
    ShapeReader* getTable(uint32_t index);

    /**
     * Get total shapes across all tables.
     */
    uint32_t getTotalShapeCount() const;

private:
    std::vector<std::vector<uint8_t>> m_packetData;
    std::vector<ShapeReader> m_tables;
};

} // namespace mcgng

#endif // MCGNG_SHAPE_READER_H
