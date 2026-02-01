#include "assets/shape_reader.h"
#include "assets/pak_reader.h"
#include <cstring>
#include <iostream>

namespace mcgng {

bool ShapeReader::load(const uint8_t* data, size_t size) {
    if (!data || size < 8) {
        return false;
    }

    m_data = data;
    m_size = size;

    // Read version string (4 bytes)
    m_version = std::string(reinterpret_cast<const char*>(data), 4);

    // Read shape count (4 bytes at offset 4)
    std::memcpy(&m_shapeCount, data + 4, sizeof(uint32_t));

    // Sanity check
    if (m_shapeCount > 10000) {
        std::cerr << "ShapeReader: Invalid shape count: " << m_shapeCount << std::endl;
        return false;
    }

    // Read offset table (8 bytes per shape, starting at offset 8)
    size_t tableSize = 8 + m_shapeCount * 8;
    if (size < tableSize) {
        std::cerr << "ShapeReader: File too small for offset table" << std::endl;
        return false;
    }

    m_offsets.resize(m_shapeCount);
    for (uint32_t i = 0; i < m_shapeCount; ++i) {
        std::memcpy(&m_offsets[i], data + 8 + i * 8, sizeof(uint32_t));
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
