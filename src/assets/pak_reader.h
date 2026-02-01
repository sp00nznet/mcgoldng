#ifndef MCGNG_PAK_READER_H
#define MCGNG_PAK_READER_H

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <functional>

namespace mcgng {

/**
 * PAK Storage Types
 *
 * The top 3 bits of each seek table entry encode the storage type.
 */
enum class PakStorageType : uint8_t {
    RAW  = 0x00,  // Uncompressed
    FWF  = 0x01,  // File within file
    LZD  = 0x02,  // LZ compressed
    HF   = 0x03,  // Huffman compressed (not supported)
    ZLIB = 0x04,  // zlib compressed
    NUL  = 0x07,  // Empty/null packet
};

/**
 * PAK Packet Entry
 */
struct PakEntry {
    uint32_t offset;            // Offset to packet data (lower 29 bits)
    PakStorageType storageType; // Storage type (upper 3 bits)
    uint32_t packedSize;        // Size of packed data
    uint32_t unpackedSize;      // Size of unpacked data
};

/**
 * PAK Archive Reader for MechCommander Gold
 *
 * PAK format:
 * - Header:
 *   - Bytes 0-3: Magic 0xFEEDFACE (PACKET_FILE_VERSION)
 *   - Bytes 4-7: First packet offset (encodes packet count)
 * - Seek Table (4 bytes per entry):
 *   - Bits 0-28: Offset
 *   - Bits 29-31: Storage type
 * - Packet data:
 *   - For compressed packets: First DWORD is uncompressed size
 *   - Then the actual data
 */
class PakReader {
public:
    static constexpr uint32_t PAK_MAGIC = 0xFEEDFACE;
    static constexpr uint32_t TYPE_SHIFT = 29;
    static constexpr uint32_t OFFSET_MASK = (1U << TYPE_SHIFT) - 1;

    PakReader() = default;
    ~PakReader();

    // Disable copy, enable move
    PakReader(const PakReader&) = delete;
    PakReader& operator=(const PakReader&) = delete;
    PakReader(PakReader&&) noexcept;
    PakReader& operator=(PakReader&&) noexcept;

    /**
     * Open a PAK archive file.
     * @param path Path to the .PAK file
     * @return true on success, false on failure
     */
    bool open(const std::string& path);

    /**
     * Close the archive.
     */
    void close();

    /**
     * Check if archive is open.
     */
    bool isOpen() const { return m_file.is_open(); }

    /**
     * Get the path of the opened archive.
     */
    const std::string& getPath() const { return m_archivePath; }

    /**
     * Get number of packets in the archive.
     */
    size_t getNumPackets() const { return m_entries.size(); }

    /**
     * Get all entries in the archive.
     */
    const std::vector<PakEntry>& getEntries() const { return m_entries; }

    /**
     * Get a specific entry by index.
     * @param index Packet index
     * @return Pointer to entry or nullptr if out of range
     */
    const PakEntry* getEntry(size_t index) const;

    /**
     * Read and decompress a packet from the archive.
     * @param index Packet index
     * @return Decompressed packet data, or empty vector on error
     */
    std::vector<uint8_t> readPacket(size_t index);

    /**
     * Read a packet without decompression (raw/packed form).
     * @param index Packet index
     * @return Raw packet data, or empty vector on error
     */
    std::vector<uint8_t> readPacketRaw(size_t index);

    /**
     * Get the storage type of a packet.
     */
    PakStorageType getStorageType(size_t index) const;

    /**
     * Get the unpacked size of a packet.
     */
    uint32_t getPacketSize(size_t index) const;

    /**
     * Extract all packets to a directory.
     * Files are named by their packet index.
     * @param outputDir Base output directory
     * @param prefix Optional filename prefix
     * @param progressCallback Optional callback for progress updates
     * @return Number of packets extracted
     */
    size_t extractAll(const std::string& outputDir,
                      const std::string& prefix = "packet_",
                      std::function<void(float, size_t)> progressCallback = nullptr);

    // Static helpers
    static PakStorageType extractType(uint32_t tableEntry) {
        return static_cast<PakStorageType>(tableEntry >> TYPE_SHIFT);
    }

    static uint32_t extractOffset(uint32_t tableEntry) {
        return tableEntry & OFFSET_MASK;
    }

private:
    std::ifstream m_file;
    std::string m_archivePath;
    std::vector<PakEntry> m_entries;
    size_t m_fileSize = 0;

    bool readSeekTable();
    std::vector<uint8_t> readRawData(uint32_t offset, uint32_t size);
};

} // namespace mcgng

#endif // MCGNG_PAK_READER_H
