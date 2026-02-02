#include "assets/nested_pak_reader.h"
#include "assets/lz_decompress.h"
#include <iostream>
#include <cstring>

namespace mcgng {

// MechSpriteSet implementation

bool MechSpriteSet::load(const uint8_t* data, size_t size) {
    if (!data || size < 8) {
        return false;
    }

    // Check for PAK magic
    uint32_t magic;
    std::memcpy(&magic, data, 4);
    if (magic != PakReader::PAK_MAGIC) {
        std::cerr << "MechSpriteSet: Invalid magic (expected 0xFEEDFACE, got 0x"
                  << std::hex << magic << std::dec << ")" << std::endl;
        return false;
    }

    // Read first offset to get packet count
    uint32_t firstOffset;
    std::memcpy(&firstOffset, data + 4, 4);
    uint32_t numPackets = (firstOffset & 0x1FFFFFFF) / 4 - 2;

    if (numPackets == 0 || numPackets > 10000) {
        std::cerr << "MechSpriteSet: Invalid packet count: " << numPackets << std::endl;
        return false;
    }

    // Read seek table
    std::vector<uint32_t> seekTable(numPackets);
    if (8 + numPackets * 4 > size) {
        std::cerr << "MechSpriteSet: Seek table extends past data" << std::endl;
        return false;
    }

    for (uint32_t i = 0; i < numPackets; ++i) {
        std::memcpy(&seekTable[i], data + 8 + i * 4, 4);
    }

    m_frameData.clear();
    m_frames.clear();

    // Process each packet (animation frame)
    int successCount = 0;
    for (uint32_t i = 0; i < numPackets; ++i) {
        uint32_t offset = seekTable[i] & 0x1FFFFFFF;
        uint32_t type = seekTable[i] >> 29;

        // Skip null packets
        if (type == 7 || offset >= size) {
            continue;
        }

        // Calculate packet size
        uint32_t packetSize;
        if (i + 1 < numPackets) {
            uint32_t nextOffset = seekTable[i + 1] & 0x1FFFFFFF;
            if (nextOffset > offset && nextOffset <= size) {
                packetSize = nextOffset - offset;
            } else {
                packetSize = static_cast<uint32_t>(size - offset);
            }
        } else {
            packetSize = static_cast<uint32_t>(size - offset);
        }

        if (packetSize == 0 || offset + packetSize > size) {
            continue;
        }

        const uint8_t* packetData = data + offset;
        std::vector<uint8_t> decompressed;

        // Handle compression
        if (type == 2) {  // LZ compressed
            // First 4 bytes are uncompressed size
            if (packetSize < 4) continue;

            uint32_t uncompSize;
            std::memcpy(&uncompSize, packetData, 4);

            if (uncompSize > 0 && uncompSize < 10 * 1024 * 1024) {  // Max 10MB
                decompressed.resize(uncompSize);
                size_t outSize = lzDecompress(packetData + 4, packetSize - 4,
                                              decompressed.data(), uncompSize);
                if (outSize != uncompSize) {
                    // Decompression failed or partial
                    decompressed.resize(outSize);
                }
            }
        } else if (type == 0) {  // Raw
            decompressed.assign(packetData, packetData + packetSize);
        } else {
            // Unknown type, skip
            continue;
        }

        if (decompressed.empty()) {
            continue;
        }

        // Try to parse as mech shape first (more specific format)
        MechShapeReader mechShape;
        if (mechShape.load(decompressed.data(), decompressed.size())) {
            m_frameData.push_back(std::move(decompressed));
            m_mechFrames.push_back(mechShape);
            m_mechFrames.back().load(m_frameData.back().data(), m_frameData.back().size());
            ++successCount;

            // Log first few successes
            if (successCount <= 5) {
                std::cout << "  -> Mech shape " << i << ": " << mechShape.getWidth()
                          << "x" << mechShape.getHeight() << std::endl;
            }

            // Limit frames for faster loading during testing
            if (successCount >= 30) {
                std::cout << "  (limiting to 30 frames for testing)" << std::endl;
                break;
            }
            continue;
        }

        // Only try standard shape table if we haven't found any mech frames
        // (mech sprites use the mech format, not standard shape tables)
        if (m_mechFrames.empty()) {
            m_frameData.push_back(std::move(decompressed));
            m_frames.emplace_back();

            if (m_frames.back().load(m_frameData.back().data(), m_frameData.back().size())) {
                ++successCount;
            } else {
                m_frames.pop_back();
                m_frameData.pop_back();
            }
        }
    }

    if (successCount > 0) {
        std::cout << "MechSpriteSet: Loaded " << successCount << " frames ("
                  << m_frames.size() << " standard, " << m_mechFrames.size() << " mech)" << std::endl;
    }

    return !m_frames.empty() || !m_mechFrames.empty();
}

const ShapeReader* MechSpriteSet::getFrame(uint32_t index) const {
    if (index >= m_frames.size()) {
        return nullptr;
    }
    return &m_frames[index];
}

const MechShapeReader* MechSpriteSet::getMechFrame(uint32_t index) const {
    if (index >= m_mechFrames.size()) {
        return nullptr;
    }
    return &m_mechFrames[index];
}

// NestedPakReader implementation

bool NestedPakReader::open(const std::string& path) {
    if (!m_pak.open(path)) {
        std::cerr << "NestedPakReader: Failed to open " << path << std::endl;
        return false;
    }

    size_t numPackets = m_pak.getNumPackets();
    std::cout << "NestedPakReader: " << path << " has " << numPackets << " mech types" << std::endl;

    m_mechSprites.clear();
    m_mechSprites.resize(numPackets);

    int loadedCount = 0;
    // Limit to first 3 mech types for faster startup
    size_t maxToLoad = std::min(numPackets, size_t(3));
    for (size_t i = 0; i < maxToLoad; ++i) {
        std::vector<uint8_t> packetData = m_pak.readPacket(i);
        if (packetData.empty()) {
            continue;
        }

        // Each packet should be a nested PAK
        if (m_mechSprites[i].load(packetData.data(), packetData.size())) {
            ++loadedCount;
            // Found one valid mech, good enough for testing
            break;
        }
    }

    std::cout << "NestedPakReader: Loaded " << loadedCount << "/" << numPackets << " mech types" << std::endl;
    return loadedCount > 0;
}

const MechSpriteSet* NestedPakReader::getMech(uint32_t index) const {
    if (index >= m_mechSprites.size()) {
        return nullptr;
    }
    if (!m_mechSprites[index].isLoaded()) {
        return nullptr;
    }
    return &m_mechSprites[index];
}

} // namespace mcgng
