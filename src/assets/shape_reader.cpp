#include "assets/shape_reader.h"
#include "assets/pak_reader.h"
#include <cstring>
#include <iostream>
#include <iomanip>
#include <set>
#include <map>
#include <algorithm>
#include <fstream>

namespace mcgng {

bool ShapeReader::load(const uint8_t* data, size_t size) {
    if (!data || size < 8) {
        return false;
    }

    m_data = data;
    m_size = size;

    // Check for standard shape table format (starts with version like "1.10")
    // Some mech sprites have a 7-byte header before the version string
    size_t headerOffset = 0;

    // Look for "1.10" or similar version string
    if (size >= 11 && data[7] == '1' && data[8] == '.' && data[9] == '1' && data[10] == '0') {
        // Mech sprite format with 7-byte header
        headerOffset = 7;
    } else if (size >= 4 && data[0] >= '0' && data[0] <= '9' && data[1] == '.') {
        // Standard format starting with version
        headerOffset = 0;
    } else {
        return false;
    }

    // Read version string (4 bytes)
    m_version = std::string(reinterpret_cast<const char*>(data + headerOffset), 4);

    // Store the header offset for later use
    m_headerOffset = headerOffset;

    // Read shape count (4 bytes after version string)
    std::memcpy(&m_shapeCount, data + headerOffset + 4, sizeof(uint32_t));

    // Sanity check
    if (m_shapeCount > 10000) {
        // Don't log - this is common for non-shape-table data
        return false;
    }

    // Read offset table (8 bytes per shape, starting after version + count)
    size_t tableStart = m_headerOffset + 8;  // version(4) + count(4)
    size_t tableSize = tableStart + m_shapeCount * 8;
    if (size < tableSize) {
        std::cerr << "ShapeReader: File too small for offset table" << std::endl;
        return false;
    }

    m_offsets.resize(m_shapeCount);
    for (uint32_t i = 0; i < m_shapeCount; ++i) {
        std::memcpy(&m_offsets[i], data + tableStart + i * 8, sizeof(uint32_t));
        // Offsets in mech sprites may be relative to headerOffset
        // Adjust if needed - for now assume they're absolute from data start
    }

    m_loaded = true;
    return true;
}

bool ShapeReader::getShapeHeader(uint32_t index, ShapeHeader& header) const {
    if (!m_loaded || index >= m_shapeCount) {
        return false;
    }

    uint32_t offset = m_offsets[index];
    if (offset + sizeof(ShapeHeader) > m_size) {
        return false;
    }

    std::memcpy(&header, m_data + offset, sizeof(ShapeHeader));
    return true;
}

bool ShapeReader::getRawShape(uint32_t index, const uint8_t** data, size_t* size) const {
    if (!m_loaded || index >= m_shapeCount || !data || !size) {
        return false;
    }

    uint32_t offset = m_offsets[index];
    if (offset >= m_size) {
        return false;
    }

    *data = m_data + offset;

    // Calculate size (to next shape or end)
    if (index + 1 < m_shapeCount) {
        *size = m_offsets[index + 1] - offset;
    } else {
        *size = m_size - offset;
    }

    return true;
}

ShapeData ShapeReader::decodeShape(uint32_t index) const {
    ShapeData result;

    if (!m_loaded || index >= m_shapeCount) {
        return result;
    }

    ShapeHeader header;
    if (!getShapeHeader(index, header)) {
        return result;
    }

    // Calculate dimensions
    result.width = header.xmax - header.xmin + 1;
    result.height = header.ymax - header.ymin + 1;

    // Extract hotspot from origin field
    // Origin is packed: (x << 16) | y or similar
    result.hotspotX = static_cast<int16_t>(header.origin >> 16);
    result.hotspotY = static_cast<int16_t>(header.origin & 0xFFFF);

    if (result.width <= 0 || result.height <= 0 ||
        result.width > 1024 || result.height > 1024) {
        std::cerr << "ShapeReader: Invalid shape dimensions: "
                  << result.width << "x" << result.height << std::endl;
        return ShapeData{};
    }

    // Allocate pixel buffer (initialized to 0 = transparent)
    result.pixels.resize(result.width * result.height, 0);

    // Get RLE data (starts after header)
    uint32_t offset = m_offsets[index] + sizeof(ShapeHeader);
    if (offset >= m_size) {
        return ShapeData{};
    }

    const uint8_t* rleData = m_data + offset;
    size_t rleSize = m_size - offset;

    // For next shape, limit size
    if (index + 1 < m_shapeCount && m_offsets[index + 1] > offset) {
        rleSize = m_offsets[index + 1] - offset;
    }

    // Decode RLE
    if (!decodeRLE(rleData, rleSize, result.pixels.data(), result.width, result.height)) {
        std::cerr << "ShapeReader: Failed to decode RLE for shape " << index << std::endl;
        // Return partial result anyway
    }

    return result;
}

bool ShapeReader::decodeRLE(const uint8_t* src, size_t srcSize,
                            uint8_t* dest, int width, int height) const {
    /*
     * RLE format:
     * 0: End of line
     * 1: Skip packet - next byte is count of transparent pixels
     * Marker byte with bit 0 = 0: Run packet - repeat next byte (marker >> 1) times
     * Marker byte with bit 0 = 1: String packet - copy (marker >> 1) literal bytes
     */

    // Debug: dump first 20 bytes of RLE data
    static int debugCount = 0;
    if (debugCount++ < 2) {
        std::cout << "ShapeReader RLE (" << width << "x" << height << "): ";
        for (size_t i = 0; i < std::min(srcSize, size_t(20)); ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(src[i]) << " ";
        }
        std::cout << std::dec << std::endl;
    }

    size_t srcPos = 0;
    int x = 0;
    int y = 0;

    while (srcPos < srcSize && y < height) {
        uint8_t marker = src[srcPos++];

        if (marker == 0) {
            // End of line
            x = 0;
            y++;
            continue;
        }

        if (marker == 1) {
            // Skip packet - transparent pixels
            if (srcPos >= srcSize) break;
            uint8_t count = src[srcPos++];
            x += count;
            continue;
        }

        uint8_t count = marker >> 1;

        if (marker & 1) {
            // String packet - literal bytes
            for (int i = 0; i < count && srcPos < srcSize; ++i) {
                if (x < width && y < height) {
                    dest[y * width + x] = src[srcPos];
                }
                x++;
                srcPos++;
            }
        } else {
            // Run packet - repeat single byte
            if (srcPos >= srcSize) break;
            uint8_t value = src[srcPos++];
            for (int i = 0; i < count; ++i) {
                if (x < width && y < height) {
                    dest[y * width + x] = value;
                }
                x++;
            }
        }
    }

    return true;
}

// MechShapeReader implementation

bool MechShapeReader::load(const uint8_t* data, size_t size) {
    if (!data || size < 15) {
        return false;
    }

    // MCG mech sprites format (from ModDB/thegameengine.org docs):
    // Bytes 0-1: Mech type (00 01 = Torso, etc.)
    // Bytes 2-3: Animation index or width
    // Bytes 4-5: Padding or height
    // Byte 6: Padding (00)
    // Bytes 7-10: Version "1.10"
    // Bytes 11+: Shape table (count + offsets + shape data)
    //
    // Key insight: "Delete the first 6 bytes" makes it a standard SHP
    // But the version starts at byte 7, so there's 7 bytes of header

    // Check for version "1.10" at offset 7
    if (size > 11 && data[7] == '1' && data[8] == '.' && data[9] == '1' && data[10] == '0') {
        m_version = "1.10";
        m_headerOffset = 7;  // Standard shape table starts at byte 7
    } else if (size > 10 && data[6] == '1' && data[7] == '.' && data[8] == '1' && data[9] == '0') {
        m_version = "1.10";
        m_headerOffset = 6;  // Standard shape table starts at byte 6
    } else {
        return false;
    }

    // Read dimensions from bytes 2-5 (BIG-ENDIAN) - these are in the mech header
    uint16_t width = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    uint16_t height = (static_cast<uint16_t>(data[4]) << 8) | data[5];

    if (width == 0 || height == 0 || width > 256 || height > 256) {
        return false;
    }

    m_data = data;
    m_size = size;
    m_width = width;
    m_height = height;
    m_loaded = true;

    return true;
}

ShapeData MechShapeReader::decode() const {
    ShapeData result;

    if (!m_loaded || !m_data) {
        std::cerr << "MechShapeReader::decode() - not loaded" << std::endl;
        return result;
    }

    // MCG mech sprite format analysis:
    // Bytes 0-1: Format ID (0x0100)
    // Bytes 2-3: Width (big-endian)
    // Bytes 4-5: Height (big-endian)
    // Byte 6: Padding (0x00)
    // Bytes 7-10: Version "1.10"
    // Bytes 11+: Pixel data (format TBD)
    //
    // The data starting at byte 11 does NOT follow standard VFX RLE format.
    // Run counts like 0xD8=108 pixels in a 26-wide row indicate different encoding.

    result.width = m_width;
    result.height = m_height;
    result.hotspotX = m_width / 2;
    result.hotspotY = m_height / 2;
    result.pixels.resize(result.width * result.height, 0);

    // Debug: save raw binary data and analyze
    static int hexDumpCount = 0;
    if (hexDumpCount++ < 1) {
        std::cout << "MechShapeReader: Raw data (" << m_size << " bytes)" << std::endl;

        // Save raw binary for external analysis
        {
            std::ofstream bin("mech_sprite_raw.bin", std::ios::binary);
            bin.write(reinterpret_cast<const char*>(m_data), m_size);
            std::cout << "  Saved: mech_sprite_raw.bin" << std::endl;
        }

        // Print first 80 bytes in hex for analysis
        std::cout << "  First 80 bytes:" << std::endl << "  ";
        for (size_t i = 0; i < std::min(m_size, size_t(80)); ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(m_data[i]) << " ";
            if ((i + 1) % 20 == 0) std::cout << std::endl << "  ";
        }
        std::cout << std::dec << std::endl;
    }

    // Analyze the data structure - look for row table pattern "NN 02 00 XX"
    // Find where row entries start
    size_t tableStart = 0;
    for (size_t i = 11; i + 4 < m_size; ++i) {
        // Look for sequential row numbers with 0x02 0x00 pattern
        if (m_data[i+1] == 0x02 && m_data[i+2] == 0x00 &&
            i + 8 < m_size &&
            m_data[i+4] == m_data[i] + 1 &&  // Next row number
            m_data[i+5] == 0x02 && m_data[i+6] == 0x00) {
            tableStart = i;
            break;
        }
    }

    // Scan table region for row entries with pattern "NN 02 00 XX"
    std::vector<std::pair<int, uint16_t>> rowEntries;  // (rowNum, 16-bit offset)
    std::set<int> foundRows;

    // Limit search to table region
    size_t tableEndEstimate = std::min(m_size, size_t(11 + m_height * 6));

    for (size_t pos = 11; pos + 4 < tableEndEstimate; ++pos) {
        uint8_t rowNum = m_data[pos];
        // Check for pattern: row_num, 0x02, 0x00, offset_lo, offset_hi (sometimes just offset_lo)
        if (rowNum < m_height && m_data[pos+1] == 0x02 && m_data[pos+2] == 0x00) {
            // Use 16-bit little-endian offset from bytes 3-4, but fall back to 8-bit if out of range
            uint16_t offset = m_data[pos+3];
            if (pos + 4 < m_size) {
                uint16_t offset16 = m_data[pos+3] | (m_data[pos+4] << 8);
                // If 16-bit offset is reasonable (less than file size), use it
                if (offset16 < m_size) {
                    offset = offset16;
                }
            }

            // Only add if we haven't seen this row yet
            if (foundRows.find(rowNum) == foundRows.end()) {
                // Validate: skip entries that look like false positives
                bool isValidEntry = true;
                if (!rowEntries.empty()) {
                    int lastRowNum = rowEntries.back().first;
                    // If we jump backwards by more than 3 rows, likely a false positive
                    if (rowNum < lastRowNum && lastRowNum - rowNum > 3) {
                        isValidEntry = false;
                    }
                }

                if (isValidEntry) {
                    rowEntries.push_back({rowNum, offset});
                    foundRows.insert(rowNum);
                }
            }
        }
    }

    // Debug: output row count
    static int mechDebugCount = 0;
    if (mechDebugCount++ < 1) {
        std::cout << "MechShapeReader: Found " << rowEntries.size() << " row entries" << std::endl;
    }

    // Data starts at byte 11 (after 7-byte prefix + 4-byte version)
    size_t dataOffset = 11;
    if (dataOffset >= m_size) {
        return result;
    }

    const uint8_t* pixelData = m_data + dataOffset;
    size_t dataSize = m_size - dataOffset;

    // Try multiple decoding strategies and pick the one that produces best results

    // Strategy 0: Use row offset table for simple raw bytes decode
    // The offsets point into the data section - just read pixels directly
    // Format appears to be: each row's data starts at offset, read until end marker
    {
        std::cout << "MechShapeReader: Analyzing row offset table..." << std::endl;

        // Build row offset map from the table we found
        std::map<int, size_t> rowOffsets;  // row -> file offset

        // Scan for row entries with pattern "NN 02 00 XX"
        for (size_t pos = 11; pos + 4 < m_size && pos < 200; ++pos) {
            uint8_t rowNum = m_data[pos];
            if (rowNum < m_height && m_data[pos+1] == 0x02 && m_data[pos+2] == 0x00) {
                uint8_t offsetFromEnd = m_data[pos+3];
                if (offsetFromEnd > 0 && offsetFromEnd < 250) {
                    size_t fileOffset = m_size - offsetFromEnd;
                    if (fileOffset > 100 && fileOffset < m_size && rowOffsets.find(rowNum) == rowOffsets.end()) {
                        rowOffsets[rowNum] = fileOffset;
                    }
                }
            }
        }

        std::cout << "  Found " << rowOffsets.size() << " row offsets" << std::endl;

        // Sort offsets to find data regions
        std::vector<std::pair<size_t, int>> sortedOffsets;
        for (auto& entry : rowOffsets) {
            sortedOffsets.push_back({entry.second, entry.first});
        }
        std::sort(sortedOffsets.begin(), sortedOffsets.end());

        // The rows overlap in the file, suggesting RLE compression
        // Try different RLE approaches starting from earliest offset
        if (!sortedOffsets.empty()) {
            size_t dataStart = sortedOffsets.front().first;
            std::cout << "  Data region starts at offset " << dataStart << std::endl;

            // Try simple VFX RLE from data start
            std::vector<uint8_t> rlePixels(m_width * m_height, 0);
            const uint8_t* rle = m_data + dataStart;
            size_t rleSize = m_size - dataStart;
            size_t srcPos = 0;
            int x = 0, y = 0;

            while (y < m_height && srcPos < rleSize) {
                uint8_t marker = rle[srcPos++];

                if (marker == 0) {
                    x = 0; y++;
                } else if (marker == 1) {
                    if (srcPos >= rleSize) break;
                    x += rle[srcPos++];
                } else if ((marker & 1) == 0) {
                    if (srcPos >= rleSize) break;
                    uint8_t color = rle[srcPos++];
                    int count = marker >> 1;
                    for (int i = 0; i < count && x < m_width; ++i) {
                        if (y < m_height) rlePixels[y * m_width + x++] = color;
                    }
                } else {
                    int count = marker >> 1;
                    for (int i = 0; i < count && srcPos < rleSize && x < m_width; ++i) {
                        if (y < m_height) rlePixels[y * m_width + x++] = rle[srcPos++];
                    }
                }
            }

            int nonZero = 0;
            for (uint8_t p : rlePixels) if (p != 0) nonZero++;

            std::cout << "  VFX RLE from " << dataStart << ": " << nonZero << "/" << rlePixels.size()
                      << " pixels, " << y << " rows" << std::endl;

            if (nonZero > m_width * m_height / 4 && y >= m_height / 2) {
                result.pixels = std::move(rlePixels);
                static int rleSaveCount = 0;
                if (rleSaveCount++ < 1) {
                    std::string filename = "mech_vfxrle_" + std::to_string(m_width) + "x" + std::to_string(m_height) + ".pgm";
                    std::ofstream pgm(filename, std::ios::binary);
                    if (pgm.is_open()) {
                        pgm << "P5\n" << m_width << " " << m_height << "\n255\n";
                        pgm.write(reinterpret_cast<const char*>(result.pixels.data()), result.pixels.size());
                        std::cout << "  Saved: " << filename << std::endl;
                    }
                }
                std::cout << "MechShapeReader: Using VFX RLE from data region" << std::endl;
                return result;
            }
        }
    }

    // Strategy 1: Scan all offsets for best VFX RLE decode
    {
        std::cout << "MechShapeReader: Scanning for VFX RLE start..." << std::endl;

        int bestNonZero = 0;
        size_t bestOffset = 0;
        std::vector<uint8_t> bestPixels;
        int bestScore = 0;  // Score based on distribution

        // Focus on likely data region (after header/table, before end)
        for (size_t rleStart = 100; rleStart < m_size - 100; rleStart += 1) {
            std::vector<uint8_t> vfxPixels(m_width * m_height, 0);
            const uint8_t* rle = m_data + rleStart;
            size_t rleSize = m_size - rleStart;
            size_t srcPos = 0;
            int x = 0, y = 0;
            bool valid = true;

            while (y < m_height && srcPos < rleSize && valid) {
                uint8_t marker = rle[srcPos++];

                if (marker == 0) {
                    x = 0;
                    y++;
                } else if (marker == 1) {
                    if (srcPos >= rleSize) { valid = false; break; }
                    int skip = rle[srcPos++];
                    if (skip > m_width) { valid = false; break; }
                    x += skip;
                } else if ((marker & 1) == 0) {
                    if (srcPos >= rleSize) { valid = false; break; }
                    uint8_t color = rle[srcPos++];
                    int count = marker >> 1;
                    if (x + count > m_width + 5) { valid = false; break; }
                    for (int i = 0; i < count && x < m_width; ++i) {
                        if (y < m_height) vfxPixels[y * m_width + x++] = color;
                    }
                } else {
                    int count = marker >> 1;
                    if (x + count > m_width + 5) { valid = false; break; }
                    for (int i = 0; i < count && srcPos < rleSize && x < m_width; ++i) {
                        if (y < m_height) vfxPixels[y * m_width + x++] = rle[srcPos++];
                    }
                }
            }

            if (!valid || y < m_height / 2) continue;

            int nonZero = 0;
            for (uint8_t p : vfxPixels) if (p != 0) nonZero++;

            // Look for diamond-like distribution (more in middle rows)
            int topQuarter = 0, midHalf = 0, bottomQuarter = 0;
            for (int row = 0; row < m_height / 4; ++row)
                for (int col = 0; col < m_width; ++col)
                    if (vfxPixels[row * m_width + col] != 0) topQuarter++;
            for (int row = m_height / 4; row < 3 * m_height / 4; ++row)
                for (int col = 0; col < m_width; ++col)
                    if (vfxPixels[row * m_width + col] != 0) midHalf++;
            for (int row = 3 * m_height / 4; row < m_height; ++row)
                for (int col = 0; col < m_width; ++col)
                    if (vfxPixels[row * m_width + col] != 0) bottomQuarter++;

            // Diamond shape: middle should have most pixels
            bool diamondLike = midHalf > topQuarter && midHalf > bottomQuarter;

            if (nonZero > bestNonZero && diamondLike && nonZero > m_width * m_height / 5) {
                bestNonZero = nonZero;
                bestOffset = rleStart;
                bestPixels = std::move(vfxPixels);
            }
        }

        if (bestNonZero > 0) {
            std::cout << "  Best offset " << bestOffset << ": " << bestNonZero << "/" << (m_width * m_height) << std::endl;
            result.pixels = std::move(bestPixels);

            static int scanSaveCount = 0;
            if (scanSaveCount++ < 1) {
                std::string filename = "mech_scan_" + std::to_string(m_width) + "x" + std::to_string(m_height) + ".pgm";
                std::ofstream pgm(filename, std::ios::binary);
                if (pgm.is_open()) {
                    pgm << "P5\n" << m_width << " " << m_height << "\n255\n";
                    pgm.write(reinterpret_cast<const char*>(result.pixels.data()), result.pixels.size());
                    std::cout << "  Saved: " << filename << std::endl;
                }
            }

            std::cout << "MechShapeReader: Using scan result from offset " << bestOffset << std::endl;
            return result;
        }
    }

    // Strategy 1: Raw pixels from end of file
    if (m_size >= 676) {
        size_t pixelStart = m_size - 676;
        std::vector<uint8_t> rawPixels(m_width * m_height, 0);
        std::memcpy(rawPixels.data(), m_data + pixelStart, m_width * m_height);

        int nonZero = 0;
        for (uint8_t p : rawPixels) if (p != 0) nonZero++;

        std::cout << "MechShapeReader: Raw from end: " << nonZero << "/" << rawPixels.size() << std::endl;

        if (nonZero > 100) {
            result.pixels = std::move(rawPixels);
            return result;
        }
    }

    std::cout << "MechShapeReader: Checking for embedded shape table..." << std::endl;

    // Strategy 0b: MCG-specific format with skip-run pairs
    // Looking at pattern: 00 1a 1a 00 d8 1a 00 d8 1a 1a 00 b9...
    // Hypothesis: pairs of (skip_count, literal_count) then literal pixels
    // 00 1a = skip 0, then 26 literals
    // After 26 literals, next pair...
    std::cout << "MechShapeReader: Trying skip-literal pairs..." << std::endl;
    {
        std::vector<uint8_t> pairPixels(m_width * m_height, 0);
        size_t srcPos = 0;
        int pixelIndex = 0;  // Linear pixel index
        int totalPixels = m_width * m_height;

        while (srcPos + 1 < dataSize && pixelIndex < totalPixels) {
            uint8_t skipCount = pixelData[srcPos++];
            uint8_t literalCount = pixelData[srcPos++];

            // Skip transparent pixels (advance pixel index)
            pixelIndex += skipCount;
            if (pixelIndex >= totalPixels) break;

            // Read literal pixels
            for (int i = 0; i < literalCount && srcPos < dataSize && pixelIndex < totalPixels; ++i) {
                pairPixels[pixelIndex] = pixelData[srcPos];
                pixelIndex++;
                srcPos++;
            }
        }

        // Calculate y position for debug
        int y = pixelIndex / m_width;

        int nonZero = 0;
        for (uint8_t p : pairPixels) if (p != 0) nonZero++;

        std::cout << "  Skip-literal pairs: " << nonZero << "/" << pairPixels.size()
                  << " non-zero, processed to pixel " << pixelIndex << "/" << totalPixels << std::endl;

        // If this gives reasonable coverage
        if (nonZero > m_width * m_height / 3) {
            result.pixels = std::move(pairPixels);
            std::cout << "MechShapeReader: Using skip-literal pairs" << std::endl;

            // Debug: show row distribution
            std::cout << "  Row counts: ";
            for (int row = 0; row < std::min(m_height, 8); ++row) {
                int cnt = 0;
                for (int col = 0; col < m_width; ++col) {
                    if (result.pixels[row * m_width + col] != 0) cnt++;
                }
                std::cout << cnt << " ";
            }
            std::cout << "..." << std::endl;

            // Debug: show first 8 pixels of a few rows
            std::cout << "  Row 0: ";
            for (int col = 0; col < std::min(m_width, 8); ++col) {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(result.pixels[col]) << " ";
            }
            std::cout << std::dec << std::endl;
            if (m_height > 1) {
                std::cout << "  Row 1: ";
                for (int col = 0; col < std::min(m_width, 8); ++col) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(result.pixels[m_width + col]) << " ";
                }
                std::cout << std::dec << std::endl;
            }

            return result;
        }
    }

    // Strategy 0c: Row-based RLE with (skip, run) packets per row
    // Each row: skip_count, then RLE packets until end-of-row marker
    std::cout << "MechShapeReader: Trying row-based skip-run..." << std::endl;
    {
        std::vector<uint8_t> rowSkipPixels(m_width * m_height, 0);
        size_t srcPos = 0;

        for (int y = 0; y < m_height && srcPos < dataSize; ++y) {
            int x = 0;

            // Process packets until row is full or we hit a 0-length marker
            while (x < m_width && srcPos < dataSize) {
                uint8_t marker = pixelData[srcPos++];

                if (marker == 0) {
                    // End of row or skip to end
                    break;
                }

                // Check if high bit set (RLE run vs literal)
                if (marker >= 128) {
                    // RLE: repeat next byte (256-marker) or (marker-128) times
                    if (srcPos >= dataSize) break;
                    uint8_t value = pixelData[srcPos++];
                    int count = marker - 128;  // Could also try 256 - marker
                    for (int i = 0; i < count && x < m_width; ++i) {
                        rowSkipPixels[y * m_width + x++] = value;
                    }
                } else {
                    // Literal: copy marker bytes
                    for (int i = 0; i < marker && srcPos < dataSize && x < m_width; ++i) {
                        rowSkipPixels[y * m_width + x++] = pixelData[srcPos++];
                    }
                }
            }
        }

        int nonZero = 0;
        for (uint8_t p : rowSkipPixels) if (p != 0) nonZero++;

        std::cout << "  Row-based skip-run: " << nonZero << "/" << rowSkipPixels.size()
                  << " non-zero" << std::endl;

        if (nonZero > m_width * m_height / 3) {
            result.pixels = std::move(rowSkipPixels);
            std::cout << "MechShapeReader: Using row-based skip-run" << std::endl;

            // Debug: show row distribution
            std::cout << "  Row counts: ";
            for (int row = 0; row < std::min(m_height, 10); ++row) {
                int cnt = 0;
                for (int col = 0; col < m_width; ++col) {
                    if (result.pixels[row * m_width + col] != 0) cnt++;
                }
                std::cout << cnt << " ";
            }
            std::cout << "..." << std::endl;

            // Save decoded sprite as PGM (grayscale) for visual inspection
            static int saveCount = 0;
            if (saveCount++ < 1) {
                std::string filename = "mech_debug_" + std::to_string(m_width) + "x" + std::to_string(m_height) + ".pgm";
                std::ofstream pgm(filename, std::ios::binary);
                if (pgm.is_open()) {
                    pgm << "P5\n" << m_width << " " << m_height << "\n255\n";
                    pgm.write(reinterpret_cast<const char*>(result.pixels.data()), result.pixels.size());
                    std::cout << "  Saved debug image: " << filename << std::endl;
                }
            }

            return result;
        }
    }

    // Strategy 1: Uncompressed indexed pixels (most straightforward)
    // If dataSize >= width*height, could be direct pixel data
    if (dataSize >= static_cast<size_t>(m_width * m_height)) {
        std::cout << "MechShapeReader: Trying uncompressed pixels..." << std::endl;
        std::vector<uint8_t> uncompPixels(m_width * m_height);
        std::memcpy(uncompPixels.data(), pixelData, m_width * m_height);

        int nonZero = 0;
        for (uint8_t p : uncompPixels) if (p != 0) nonZero++;

        // Check if this looks like valid pixel data (should have varied values)
        std::set<uint8_t> uniqueValues(uncompPixels.begin(), uncompPixels.end());
        std::cout << "  Uncompressed: " << nonZero << " non-zero, "
                  << uniqueValues.size() << " unique values" << std::endl;

        // Don't use uncompressed if row-prefix looks better
        // (This is fallback only)
    }

    // Strategy 2: MCG-specific RLE (different from VFX standard)
    // Format hypothesis: Each row starts fresh, counts are limited to row width
    // 0 = end of row
    // 1-127 = copy N literal bytes
    // 128-255 = repeat next byte (256-N) times
    std::cout << "MechShapeReader: Trying MCG RLE format..." << std::endl;
    {
        std::vector<uint8_t> rlePixels(m_width * m_height, 0);
        size_t srcPos = 0;
        int x = 0;
        int y = 0;

        while (srcPos < dataSize && y < m_height) {
            uint8_t marker = pixelData[srcPos++];

            if (marker == 0) {
                // End of row
                x = 0;
                y++;
                continue;
            }

            if (marker < 128) {
                // Copy N literal bytes
                int count = marker;
                for (int i = 0; i < count && srcPos < dataSize && x < m_width; ++i) {
                    rlePixels[y * m_width + x] = pixelData[srcPos++];
                    x++;
                }
            } else {
                // Run-length: repeat next byte (256 - marker) times
                // Or could be (marker - 128) times - try both
                if (srcPos >= dataSize) break;
                uint8_t value = pixelData[srcPos++];
                int count = 256 - marker;  // e.g., 0xD8 (216) -> 40 pixels

                for (int i = 0; i < count && x < m_width; ++i) {
                    rlePixels[y * m_width + x] = value;
                    x++;
                }
            }
        }

        int nonZero = 0;
        for (uint8_t p : rlePixels) if (p != 0) nonZero++;

        std::cout << "  MCG RLE (256-N): " << nonZero << "/" << rlePixels.size()
                  << " non-zero pixels" << std::endl;

        // If this gives reasonable results, use it
        if (nonZero > m_width * m_height / 4) {
            result.pixels = std::move(rlePixels);
            return result;
        }
    }

    // Strategy 3: Standard VFX RLE (original attempt)
    std::cout << "MechShapeReader: Trying standard VFX RLE..." << std::endl;
    {
        std::vector<uint8_t> vfxPixels(m_width * m_height, 0);
        size_t srcPos = 0;
        int x = 0;
        int y = 0;

        while (srcPos < dataSize && y < m_height) {
            uint8_t marker = pixelData[srcPos++];

            if (marker == 0) {
                x = 0;
                y++;
                continue;
            }

            if (marker == 1) {
                if (srcPos >= dataSize) break;
                uint8_t count = pixelData[srcPos++];
                x += count;
                continue;
            }

            uint8_t count = marker >> 1;

            if (marker & 1) {
                // String packet
                for (int i = 0; i < count && srcPos < dataSize; ++i) {
                    if (x < m_width && y < m_height) {
                        vfxPixels[y * m_width + x] = pixelData[srcPos];
                    }
                    x++;
                    srcPos++;
                }
            } else {
                // Run packet
                if (srcPos >= dataSize) break;
                uint8_t value = pixelData[srcPos++];
                for (int i = 0; i < count; ++i) {
                    if (x < m_width && y < m_height) {
                        vfxPixels[y * m_width + x] = value;
                    }
                    x++;
                }
            }
        }

        int nonZero = 0;
        for (uint8_t p : vfxPixels) if (p != 0) nonZero++;
        std::cout << "  VFX RLE: " << nonZero << "/" << vfxPixels.size() << " non-zero" << std::endl;

        result.pixels = std::move(vfxPixels);
    }

    // Debug: show pixel distribution per row
    std::cout << "MechShapeReader: Row pixel counts: ";
    for (int y = 0; y < std::min(m_height, 10); ++y) {
        int count = 0;
        for (int x = 0; x < m_width; ++x) {
            if (result.pixels[y * m_width + x] != 0) count++;
        }
        std::cout << count << " ";
    }
    std::cout << "..." << std::endl;

    // Check if image might need vertical flip (common issue)
    int topHalfNonZero = 0, bottomHalfNonZero = 0;
    for (int y = 0; y < m_height / 2; ++y) {
        for (int x = 0; x < m_width; ++x) {
            if (result.pixels[y * m_width + x] != 0) topHalfNonZero++;
        }
    }
    for (int y = m_height / 2; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            if (result.pixels[y * m_width + x] != 0) bottomHalfNonZero++;
        }
    }
    std::cout << "MechShapeReader: Top half: " << topHalfNonZero
              << ", Bottom half: " << bottomHalfNonZero << std::endl;

    return result;
}

// ShapePackReader implementation

bool ShapePackReader::loadFromPak(const std::string& pakPath) {
    PakReader pak;
    if (!pak.open(pakPath)) {
        std::cerr << "ShapePackReader: Failed to open PAK: " << pakPath << std::endl;
        return false;
    }

    size_t packetCount = pak.getNumPackets();
    if (packetCount == 0) {
        std::cerr << "ShapePackReader: No packets in PAK" << std::endl;
        return false;
    }

    m_packetData.clear();
    m_tables.clear();

    for (size_t i = 0; i < packetCount; ++i) {
        std::vector<uint8_t> data = pak.readPacket(i);
        if (data.empty()) {
            continue;
        }

        // Check if this looks like a shape table (version check)
        // Version should be something like "1.10" or similar ASCII
        if (data.size() >= 8) {
            bool looksLikeShapeTable = true;
            for (int j = 0; j < 4; ++j) {
                char c = static_cast<char>(data[j]);
                if (c != '.' && (c < '0' || c > '9')) {
                    looksLikeShapeTable = false;
                    break;
                }
            }

            if (looksLikeShapeTable) {
                m_packetData.push_back(std::move(data));
                m_tables.emplace_back();
                m_tables.back().load(m_packetData.back().data(), m_packetData.back().size());
            }
        }
    }

    if (m_tables.empty()) {
        // Maybe the whole PAK is one shape table
        std::vector<uint8_t> allData = pak.readPacket(0);
        if (!allData.empty() && allData.size() >= 8) {
            m_packetData.push_back(std::move(allData));
            m_tables.emplace_back();
            if (!m_tables.back().load(m_packetData.back().data(), m_packetData.back().size())) {
                m_tables.pop_back();
                m_packetData.pop_back();
            }
        }
    }

    return !m_tables.empty();
}

ShapeReader* ShapePackReader::getTable(uint32_t index) {
    if (index >= m_tables.size()) {
        return nullptr;
    }
    return &m_tables[index];
}

uint32_t ShapePackReader::getTotalShapeCount() const {
    uint32_t total = 0;
    for (const auto& table : m_tables) {
        total += table.getShapeCount();
    }
    return total;
}

} // namespace mcgng
