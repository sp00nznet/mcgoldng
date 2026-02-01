#include "assets/pak_reader.h"
#include "assets/lz_decompress.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace mcgng {

PakReader::~PakReader() {
    close();
}

PakReader::PakReader(PakReader&& other) noexcept
    : m_file(std::move(other.m_file))
    , m_archivePath(std::move(other.m_archivePath))
    , m_entries(std::move(other.m_entries))
    , m_fileSize(other.m_fileSize) {
    other.m_fileSize = 0;
}

PakReader& PakReader::operator=(PakReader&& other) noexcept {
    if (this != &other) {
        close();
        m_file = std::move(other.m_file);
        m_archivePath = std::move(other.m_archivePath);
        m_entries = std::move(other.m_entries);
        m_fileSize = other.m_fileSize;
        other.m_fileSize = 0;
    }
    return *this;
}

bool PakReader::open(const std::string& path) {
    close();

    m_file.open(path, std::ios::binary);
    if (!m_file) {
        std::cerr << "PakReader: Failed to open file: " << path << std::endl;
        return false;
    }

    // Get file size
    m_file.seekg(0, std::ios::end);
    m_fileSize = static_cast<size_t>(m_file.tellg());
    m_file.seekg(0, std::ios::beg);

    m_archivePath = path;

    if (!readSeekTable()) {
        std::cerr << "PakReader: Failed to read seek table from: " << path << std::endl;
        close();
        return false;
    }

    return true;
}

void PakReader::close() {
    if (m_file.is_open()) {
        m_file.close();
    }
    m_archivePath.clear();
    m_entries.clear();
    m_fileSize = 0;
}

bool PakReader::readSeekTable() {
    // Read and verify magic number
    uint32_t magic = 0;
    m_file.read(reinterpret_cast<char*>(&magic), sizeof(magic));

    if (m_file.fail()) {
        return false;
    }

    // Some PAK files might use checksums instead of magic
    // If magic doesn't match, we'll try to proceed anyway
    (void)(magic == PAK_MAGIC);  // hasValidMagic - not currently used

    // Read first packet offset - this encodes the number of packets
    uint32_t firstPacketOffset = 0;
    m_file.read(reinterpret_cast<char*>(&firstPacketOffset), sizeof(firstPacketOffset));

    if (m_file.fail()) {
        return false;
    }

    // Number of packets = (firstPacketOffset / 4) - 2
    // Because: 4 bytes magic + 4 bytes firstOffset + (numPackets * 4 bytes) = firstPacketOffset
    // So: numPackets = (firstPacketOffset - 8) / 4 = firstPacketOffset/4 - 2
    uint32_t numPackets = (firstPacketOffset / sizeof(uint32_t)) - 2;

    // Sanity check
    if (numPackets > 1000000 || firstPacketOffset > m_fileSize) {
        std::cerr << "PakReader: Invalid packet count: " << numPackets << std::endl;
        return false;
    }

    // Read the seek table
    std::vector<uint32_t> seekTable(numPackets);
    m_file.read(reinterpret_cast<char*>(seekTable.data()), numPackets * sizeof(uint32_t));

    if (m_file.fail()) {
        return false;
    }

    // Parse seek table entries
    m_entries.reserve(numPackets);

    for (size_t i = 0; i < numPackets; ++i) {
        PakEntry entry;
        entry.offset = extractOffset(seekTable[i]);
        entry.storageType = extractType(seekTable[i]);

        // Calculate packed size by looking at next packet offset
        uint32_t nextOffset;
        if (i + 1 < numPackets) {
            nextOffset = extractOffset(seekTable[i + 1]);
        } else {
            nextOffset = static_cast<uint32_t>(m_fileSize);
        }

        entry.packedSize = nextOffset - entry.offset;
        entry.unpackedSize = 0;  // Will be determined when reading

        // For compressed packets, read the unpacked size now
        if (entry.storageType == PakStorageType::LZD ||
            entry.storageType == PakStorageType::ZLIB) {
            auto currentPos = m_file.tellg();
            m_file.seekg(entry.offset, std::ios::beg);

            uint32_t unpackedSize = 0;
            m_file.read(reinterpret_cast<char*>(&unpackedSize), sizeof(unpackedSize));
            entry.unpackedSize = unpackedSize;

            m_file.seekg(currentPos);
        } else if (entry.storageType == PakStorageType::RAW ||
                   entry.storageType == PakStorageType::FWF) {
            entry.unpackedSize = entry.packedSize;
        }
        // NUL packets have size 0

        m_entries.push_back(entry);
    }

    return true;
}

const PakEntry* PakReader::getEntry(size_t index) const {
    if (index >= m_entries.size()) {
        return nullptr;
    }
    return &m_entries[index];
}

PakStorageType PakReader::getStorageType(size_t index) const {
    if (index >= m_entries.size()) {
        return PakStorageType::NUL;
    }
    return m_entries[index].storageType;
}

uint32_t PakReader::getPacketSize(size_t index) const {
    if (index >= m_entries.size()) {
        return 0;
    }
    return m_entries[index].unpackedSize;
}

std::vector<uint8_t> PakReader::readRawData(uint32_t offset, uint32_t size) {
    if (!m_file.is_open() || size == 0) {
        return {};
    }

    std::vector<uint8_t> data(size);
    m_file.seekg(offset, std::ios::beg);
    m_file.read(reinterpret_cast<char*>(data.data()), size);

    if (m_file.fail()) {
        m_file.clear();
        return {};
    }

    return data;
}

std::vector<uint8_t> PakReader::readPacketRaw(size_t index) {
    const PakEntry* entry = getEntry(index);
    if (!entry || entry->packedSize == 0) {
        return {};
    }

    return readRawData(entry->offset, entry->packedSize);
}

std::vector<uint8_t> PakReader::readPacket(size_t index) {
    const PakEntry* entry = getEntry(index);
    if (!entry) {
        return {};
    }

    switch (entry->storageType) {
        case PakStorageType::NUL:
            return {};

        case PakStorageType::RAW:
        case PakStorageType::FWF:
            return readRawData(entry->offset, entry->packedSize);

        case PakStorageType::LZD: {
            // Skip the uncompressed size field (4 bytes) at the start
            auto rawData = readRawData(entry->offset + sizeof(uint32_t),
                                       entry->packedSize - sizeof(uint32_t));
            if (rawData.empty()) {
                return {};
            }
            return decompress(rawData.data(), rawData.size(), entry->unpackedSize, false);
        }

        case PakStorageType::ZLIB: {
            // Skip the uncompressed size field (4 bytes) at the start
            auto rawData = readRawData(entry->offset + sizeof(uint32_t),
                                       entry->packedSize - sizeof(uint32_t));
            if (rawData.empty()) {
                return {};
            }
            return decompress(rawData.data(), rawData.size(), entry->unpackedSize, true);
        }

        case PakStorageType::HF:
            std::cerr << "PakReader: Huffman compression not supported for packet " << index << std::endl;
            return {};

        default:
            std::cerr << "PakReader: Unknown storage type for packet " << index << std::endl;
            return {};
    }
}

size_t PakReader::extractAll(const std::string& outputDir,
                              const std::string& prefix,
                              std::function<void(float, size_t)> progressCallback) {
    if (!m_file.is_open() || m_entries.empty()) {
        return 0;
    }

    // Create output directory
    std::error_code ec;
    fs::create_directories(outputDir, ec);
    if (ec) {
        std::cerr << "PakReader: Failed to create output directory: " << outputDir << std::endl;
        return 0;
    }

    size_t extracted = 0;
    size_t total = m_entries.size();

    for (size_t i = 0; i < total; ++i) {
        if (progressCallback) {
            progressCallback(static_cast<float>(i) / total, i);
        }

        const auto& entry = m_entries[i];

        // Skip null packets
        if (entry.storageType == PakStorageType::NUL) {
            continue;
        }

        // Build output filename
        std::ostringstream oss;
        oss << prefix << std::setw(5) << std::setfill('0') << i << ".bin";
        fs::path outPath = fs::path(outputDir) / oss.str();

        // Read and decompress packet
        auto data = readPacket(i);
        if (data.empty() && entry.unpackedSize > 0) {
            std::cerr << "PakReader: Failed to read packet " << i << std::endl;
            continue;
        }

        // Write to file
        std::ofstream outFile(outPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "PakReader: Failed to create file: " << outPath << std::endl;
            continue;
        }

        if (!data.empty()) {
            outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
        }

        if (!outFile.fail()) {
            ++extracted;
        }
    }

    if (progressCallback) {
        progressCallback(1.0f, total);
    }

    return extracted;
}

} // namespace mcgng
