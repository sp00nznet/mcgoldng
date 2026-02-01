#ifndef MCGNG_FST_READER_H
#define MCGNG_FST_READER_H

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <functional>

namespace mcgng {

/**
 * FST Archive Entry
 *
 * MCG FST format (differs from MC2!):
 * - No magic bytes at start
 * - No hash field in entries
 * - 262 bytes per entry:
 *   - Offset 0-3:   DataOffset (4 bytes, little-endian)
 *   - Offset 4-7:   CompressedSize (4 bytes)
 *   - Offset 8-11:  UncompressedSize (4 bytes)
 *   - Offset 12-261: FilePath (250 bytes, null-terminated, backslash separators)
 */
struct FstEntry {
    uint32_t dataOffset;        // Offset to data in the archive
    uint32_t compressedSize;    // Size when compressed (or same as uncompressed if not)
    uint32_t uncompressedSize;  // Original file size
    std::string filePath;       // File path within archive

    bool isCompressed() const {
        return compressedSize < uncompressedSize && compressedSize > 0;
    }
};

/**
 * FST Archive Reader for MechCommander Gold
 *
 * Target archives:
 * - ART.FST     - GUI, menus, pilot portraits
 * - MISSION.FST - Mission data, warrior profiles, ABL scripts
 * - MISC.FST    - Fonts, palettes, interface elements
 * - SHAPES.FST  - Sprite data
 * - TERRAIN.FST - Map terrain data
 */
class FstReader {
public:
    static constexpr size_t MAX_FILENAME_SIZE = 250;
    static constexpr size_t ENTRY_SIZE = 262;  // 4 + 4 + 4 + 250 = 262 bytes

    FstReader() = default;
    ~FstReader();

    // Disable copy, enable move
    FstReader(const FstReader&) = delete;
    FstReader& operator=(const FstReader&) = delete;
    FstReader(FstReader&&) noexcept;
    FstReader& operator=(FstReader&&) noexcept;

    /**
     * Open an FST archive file.
     * @param path Path to the .FST file
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
     * Get all entries in the archive.
     */
    const std::vector<FstEntry>& getEntries() const { return m_entries; }

    /**
     * Get number of files in the archive.
     */
    size_t getNumFiles() const { return m_entries.size(); }

    /**
     * Find an entry by path (case-insensitive).
     * @param path File path to find
     * @return Pointer to entry or nullptr if not found
     */
    const FstEntry* findEntry(const std::string& path) const;

    /**
     * Read and decompress a file from the archive.
     * @param entry Entry to read
     * @return Decompressed file data, or empty vector on error
     */
    std::vector<uint8_t> readFile(const FstEntry& entry);

    /**
     * Read and decompress a file by path.
     * @param path File path within archive
     * @return Decompressed file data, or empty vector on error
     */
    std::vector<uint8_t> readFile(const std::string& path);

    /**
     * Extract a file to disk.
     * @param entry Entry to extract
     * @param outputPath Path to write the file
     * @return true on success
     */
    bool extractFile(const FstEntry& entry, const std::string& outputPath);

    /**
     * Extract all files to a directory.
     * @param outputDir Base output directory
     * @param progressCallback Optional callback for progress updates (0.0 to 1.0)
     * @return Number of files extracted
     */
    size_t extractAll(const std::string& outputDir,
                      std::function<void(float, const std::string&)> progressCallback = nullptr);

private:
    std::ifstream m_file;
    std::string m_archivePath;
    std::vector<FstEntry> m_entries;

    bool readEntryTable();
    std::vector<uint8_t> readRawData(uint32_t offset, uint32_t size);
};

} // namespace mcgng

#endif // MCGNG_FST_READER_H
