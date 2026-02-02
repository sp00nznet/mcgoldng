#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: pak_inspect <pakfile> [packet_index]" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open: " << argv[1] << std::endl;
        return 1;
    }

    // Read header
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), 4);
    
    if (magic != 0xFEEDFACE) {
        std::cerr << "Not a PAK file (magic=" << std::hex << magic << ")" << std::endl;
        return 1;
    }

    uint32_t firstOffset;
    file.read(reinterpret_cast<char*>(&firstOffset), 4);
    
    uint32_t numPackets = (firstOffset & 0x1FFFFFFF) / 4 - 2;
    std::cout << "PAK File: " << argv[1] << std::endl;
    std::cout << "Packets: " << numPackets << std::endl;
    std::cout << std::endl;

    // Read seek table
    std::vector<uint32_t> seekTable(numPackets);
    for (uint32_t i = 0; i < numPackets; ++i) {
        file.read(reinterpret_cast<char*>(&seekTable[i]), 4);
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();

    // If specific packet requested
    int targetPacket = (argc > 2) ? std::atoi(argv[2]) : -1;

    // Find non-null packets
    int shown = 0;
    for (uint32_t i = 0; i < numPackets && shown < 10; ++i) {
        if ((seekTable[i] >> 29) == 7) continue;  // Skip null packets
        uint32_t offset = seekTable[i] & 0x1FFFFFFF;
        uint32_t type = seekTable[i] >> 29;
        
        // Calculate size
        uint32_t nextOffset = (i + 1 < numPackets) ? (seekTable[i + 1] & 0x1FFFFFFF) : fileSize;
        uint32_t size = nextOffset - offset;

        std::cout << "Packet " << i << ": offset=" << offset << " size=" << size << " type=" << type;
        
        // Read first 32 bytes
        file.seekg(offset);
        uint8_t header[32];
        file.read(reinterpret_cast<char*>(header), std::min(size, 32u));
        
        std::cout << " [";
        for (int j = 0; j < 16 && j < (int)size; ++j) {
            printf("%02X ", header[j]);
        }
        std::cout << "]" << std::endl;

        // Check for nested PAK
        uint32_t nestedMagic;
        std::memcpy(&nestedMagic, header, 4);
        if (nestedMagic == 0xFEEDFACE) {
            uint32_t nestedFirstOffset;
            std::memcpy(&nestedFirstOffset, header + 4, 4);
            uint32_t nestedPackets = (nestedFirstOffset & 0x1FFFFFFF) / 4 - 2;
            std::cout << "  -> Nested PAK with " << nestedPackets << " sub-packets" << std::endl;
            
            // Read nested seek table
            file.seekg(offset + 8);
            for (uint32_t j = 0; j < std::min(nestedPackets, 3u); ++j) {
                uint32_t entry;
                file.read(reinterpret_cast<char*>(&entry), 4);
                uint32_t subOffset = entry & 0x1FFFFFFF;
                uint32_t subType = entry >> 29;
                
                // Read sub-packet header
                file.seekg(offset + subOffset);
                uint8_t subHeader[32];
                file.read(reinterpret_cast<char*>(subHeader), 32);
                
                std::cout << "    Sub-" << j << ": offset=" << subOffset << " type=" << subType << " [";
                for (int k = 0; k < 16; ++k) {
                    printf("%02X ", subHeader[k]);
                }
                std::cout << "]" << std::endl;
            }
        }
        ++shown;
    }

    return 0;
}
