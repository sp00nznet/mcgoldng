#ifndef MCGNG_NESTED_PAK_READER_H
#define MCGNG_NESTED_PAK_READER_H

#include "assets/pak_reader.h"
#include "assets/shape_reader.h"
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace mcgng {

/**
 * Nested PAK Reader for MechCommander Gold sprite archives.
 *
 * Mech sprite PAKs (TORSOS.PAK, LEGS.PAK, etc.) have a nested structure:
 * - Outer PAK: Contains one packet per mech/unit type
 * - Each packet is itself a PAK containing animation frames
 * - Each frame may be LZ compressed with 4-byte uncompressed size header
 *
 * Structure:
 *   Outer PAK:
 *     Packet 0 -> Inner PAK (Mech Type 0)
 *       Sub-packet 0 -> Shape table (Frame 0)
 *       Sub-packet 1 -> Shape table (Frame 1)
 *       ...
 *     Packet 1 -> Inner PAK (Mech Type 1)
 *       ...
 */

/**
 * Represents a single mech/unit with all its animation frames.
 */
class MechSpriteSet {
public:
    MechSpriteSet() = default;

    /**
     * Load from an inner PAK packet (already decompressed outer packet).
     * @param data Pointer to inner PAK data
     * @param size Size of data
     * @return true on success
     */
    bool load(const uint8_t* data, size_t size);

    /**
     * Get number of frames (standard format).
     */
    uint32_t getFrameCount() const { return static_cast<uint32_t>(m_frames.size()); }

    /**
     * Get number of mech frames.
     */
    uint32_t getMechFrameCount() const { return static_cast<uint32_t>(m_mechFrames.size()); }

    /**
     * Get shape data for a specific frame (standard format).
     */
    const ShapeReader* getFrame(uint32_t index) const;

    /**
     * Get mech shape data for a specific frame (mech format).
     */
    const MechShapeReader* getMechFrame(uint32_t index) const;

    /**
     * Check if loaded.
     */
    bool isLoaded() const { return !m_frames.empty() || !m_mechFrames.empty(); }

private:
    std::vector<std::vector<uint8_t>> m_frameData;   // Decompressed frame data
    std::vector<ShapeReader> m_frames;                // Parsed shape tables (standard format)
    std::vector<MechShapeReader> m_mechFrames;        // Parsed shapes (mech format)
};

/**
 * Reads mech sprite PAK files with nested structure.
 */
class NestedPakReader {
public:
    NestedPakReader() = default;

    /**
     * Open a nested sprite PAK file.
     * @param path Path to PAK file (e.g., TORSOS.PAK)
     * @return true on success
     */
    bool open(const std::string& path);

    /**
     * Get number of mech/unit types in this PAK.
     */
    uint32_t getMechCount() const { return static_cast<uint32_t>(m_mechSprites.size()); }

    /**
     * Get sprite set for a specific mech type.
     * @param index Mech index (0-based)
     * @return Pointer to sprite set, or nullptr if invalid
     */
    const MechSpriteSet* getMech(uint32_t index) const;

    /**
     * Get the outer PAK reader.
     */
    const PakReader& getPak() const { return m_pak; }

private:
    PakReader m_pak;
    std::vector<MechSpriteSet> m_mechSprites;
};

} // namespace mcgng

#endif // MCGNG_NESTED_PAK_READER_H
