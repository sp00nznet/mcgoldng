#include "assets/fst_reader.h"
#include "assets/lz_decompress.h"
#include <algorithm>
#include <filesystem>
#include <cstring>
#include <iostream>

namespace fs = std::filesystem;

namespace mcgng {

FstReader::~FstReader() {
    close();
}

FstReader::FstReader(FstReader&& other) noexcept
    : m_file(std::move(other.m_file))
    , m_archivePath(std::move(other.m_archivePath))
    , m_entries(std::move(other.m_entries)) {
}

FstReader& FstReader::operator=(FstReader&& other) noexcept {
    if (this != &other) {
        close();
        m_file = std::move(other.m_file);
        m_archivePath = std::move(other.m_archivePath);
        m_entries = std::move(other.m_entries);
    }
    return *this;
}

bool FstReader::open(const std::string& path) {
    close();

    m_file.open(path, std::ios::binary);
    if (!m_file) {
        std::cerr << "FstReader: Failed to open file: " << path << std::endl;
        return false;
    }

    m_archivePath = path;

    if (!readEntryTable()) {
        std::cerr << "FstReader: Failed to read entry table from: " << path << std::endl;
        close();
        return false;
    }

    return true;
}

void FstReader::close() {
    if (m_file.is_open()) {
        m_file.close();
    }
    m_archivePath.clear();
    m_entries.clear();
}

bool FstReader::readEntryTable() {
    // MCG FST format starts with entry count (4 bytes, little-endian)
    uint32_t numEntries = 0;
    m_file.read(reinterpret_cast<char*>(&numEntries), sizeof(numEntries));

    if (m_file.fail()) {
        return false;
    }

    // Sanity check - if too many entries, this might not be a valid FST
    if (numEntries > 100000) {
        std::cerr << "FstReader: Suspicious entry count: " << numEntries << std::endl;
        return false;
    }

    m_entries.reserve(numEntries);

    for (uint32_t i = 0; i < numEntries; ++i) {
        FstEntry entry;
        char pathBuffer[MAX_FILENAME_SIZE + 1] = {0};

        // Read entry fields
        m_file.read(reinterpret_cast<char*>(&entry.dataOffset), sizeof(entry.dataOffset));
        m_file.read(reinterpret_cast<char*>(&entry.compressedSize), sizeof(entry.compressedSize));
        m_file.read(reinterpret_cast<char*>(&entry.uncompressedSize), sizeof(entry.uncompressedSize));
        m_file.read(pathBuffer, MAX_FILENAME_SIZE);

        if (m_file.fail()) {
            std::cerr << "FstReader: Failed reading entry " << i << std::endl;
            return false;
        }

        // Convert path - replace backslashes with forward slashes
        entry.filePath = pathBuffer;
        std::replace(entry.filePath.begin(), entry.filePath.end(), '\\', '/');

        // Trim any trailing whitespace or nulls
        while (!entry.filePath.empty() &&
               (entry.filePath.back() == '\0' ||
                entry.filePath.back() == ' ' ||
                entry.filePath.back() == '\r' ||
                entry.filePath.back() == '\n')) {
            entry.filePath.pop_back();
        }

        m_entries.push_back(std::move(entry));
    }

    return true;
}

const FstEntry* FstReader::findEntry(const std::string& path) const {
    // Normalize the search path
    std::string searchPath = path;
    std::replace(searchPath.begin(), searchPath.end(), '\\', '/');

    // Case-insensitive search
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [&searchPath](const FstEntry& entry) {
            if (entry.filePath.length() != searchPath.length()) {
                return false;
            }
            for (size_t i = 0; i < entry.filePath.length(); ++i) {
                if (std::tolower(static_cast<unsigned char>(entry.filePath[i])) !=
                    std::tolower(static_cast<unsigned char>(searchPath[i]))) {
                    return false;
                }
            }
            return true;
        });

    return (it != m_entries.end()) ? &(*it) : nullptr;
}

std::vector<uint8_t> FstReader::readRawData(uint32_t offset, uint32_t size) {
    if (!m_file.is_open() || size == 0) {
        return {};
    }

    std::vector<uint8_t> data(size);
    m_file.seekg(offset, std::ios::beg);
    m_file.read(reinterpret_cast<char*>(data.data()), size);

    if (m_file.fail()) {
        return {};
    }

    return data;
}

std::vector<uint8_t> FstReader::readFile(const FstEntry& entry) {
    if (!m_file.is_open()) {
        return {};
    }

    // Read raw (possibly compressed) data
    uint32_t readSize = entry.isCompressed() ? entry.compressedSize : entry.uncompressedSize;
    auto rawData = readRawData(entry.dataOffset, readSize);

    if (rawData.empty()) {
        return {};
    }

    // If not compressed, return as-is
    if (!entry.isCompressed()) {
        return rawData;
    }

    // Decompress using LZ
    return decompress(rawData.data(), rawData.size(), entry.uncompressedSize, false);
}

std::vector<uint8_t> FstReader::readFile(const std::string& path) {
    const FstEntry* entry = findEntry(path);
    if (!entry) {
        return {};
    }
    return readFile(*entry);
}

bool FstReader::extractFile(const FstEntry& entry, const std::string& outputPath) {
    auto data = readFile(entry);
    if (data.empty() && entry.uncompressedSize > 0) {
        return false;
    }

    // Create parent directories if needed
    fs::path outPath(outputPath);
    if (outPath.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(outPath.parent_path(), ec);
        if (ec) {
            std::cerr << "FstReader: Failed to create directory: "
                      << outPath.parent_path() << std::endl;
            return false;
        }
    }

    // Write file
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        std::cerr << "FstReader: Failed to create output file: " << outputPath << std::endl;
        return false;
    }

    if (!data.empty()) {
        outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    return !outFile.fail();
}

size_t FstReader::extractAll(const std::string& outputDir,
                              std::function<void(float, const std::string&)> progressCallback) {
    if (!m_file.is_open() || m_entries.empty()) {
        return 0;
    }

    size_t extracted = 0;
    size_t total = m_entries.size();

    for (size_t i = 0; i < total; ++i) {
        const auto& entry = m_entries[i];

        // Build output path
        fs::path outPath = fs::path(outputDir) / entry.filePath;

        if (progressCallback) {
            progressCallback(static_cast<float>(i) / total, entry.filePath);
        }

        if (extractFile(entry, outPath.string())) {
            ++extracted;
        } else {
            std::cerr << "FstReader: Failed to extract: " << entry.filePath << std::endl;
        }
    }

    if (progressCallback) {
        progressCallback(1.0f, "Complete");
    }

    return extracted;
}

} // namespace mcgng
