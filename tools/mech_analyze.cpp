#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

// Minimal PAK reader for extracting mech data
bool readPakPacket(const char* pakPath, int outerIdx, int innerIdx, std::vector<uint8_t>& data) {
    std::ifstream f(pakPath, std::ios::binary);
    if (!f) return false;

    // Read outer PAK header
    uint32_t magic;
    f.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != 0xFEEDFACE) return false;

    uint32_t firstOffset;
    f.read(reinterpret_cast<char*>(&firstOffset), 4);
    uint32_t outerCount = (firstOffset - 8) / 4;

    if (outerIdx >= (int)outerCount) return false;

    // Read outer seek table
    std::vector<uint32_t> outerSeek(outerCount);
    for (uint32_t i = 0; i < outerCount; ++i) {
        f.seekg(8 + i * 4);
        f.read(reinterpret_cast<char*>(&outerSeek[i]), 4);
    }

    // Get outer packet
    uint32_t outerOff = outerSeek[outerIdx] & 0x1FFFFFFF;
    uint32_t outerEnd = (outerIdx + 1 < outerCount) ? (outerSeek[outerIdx + 1] & 0x1FFFFFFF) : 0;

    f.seekg(outerOff);

    // Read inner PAK from outer packet
    uint32_t innerMagic;
    f.read(reinterpret_cast<char*>(&innerMagic), 4);
    if (innerMagic != 0xFEEDFACE) {
        std::cerr << "Inner PAK magic mismatch" << std::endl;
        return false;
    }

    uint32_t innerFirstOff;
    f.read(reinterpret_cast<char*>(&innerFirstOff), 4);
    uint32_t innerCount = (innerFirstOff - 8) / 4;

    if (innerIdx >= (int)innerCount) return false;

    // Read inner seek table
    std::vector<uint32_t> innerSeek(innerCount);
    for (uint32_t i = 0; i < innerCount; ++i) {
        f.seekg(outerOff + 8 + i * 4);
        f.read(reinterpret_cast<char*>(&innerSeek[i]), 4);
    }

    uint32_t pktOff = innerSeek[innerIdx] & 0x1FFFFFFF;
    uint32_t pktEnd = (innerIdx + 1 < innerCount) ? (innerSeek[innerIdx + 1] & 0x1FFFFFFF) : (outerEnd - outerOff);

    if (pktEnd <= pktOff) return false;

    uint32_t pktSize = pktEnd - pktOff;
    data.resize(pktSize);
    f.seekg(outerOff + pktOff);
    f.read(reinterpret_cast<char*>(data.data()), pktSize);

    return true;
}

int main(int argc, char* argv[]) {
    const char* pakPath = "D:/mcg/DATA/SPRITES/TORSOS.PAK";
    std::vector<uint8_t> data;

    if (!readPakPacket(pakPath, 0, 0, data)) {
        std::cerr << "Failed to read packet" << std::endl;
        return 1;
    }

    std::cout << "Mech sprite data: " << data.size() << " bytes" << std::endl;

    // Save raw data
    std::ofstream raw("mech_sprite_raw.bin", std::ios::binary);
    raw.write(reinterpret_cast<char*>(data.data()), data.size());
    raw.close();
    std::cout << "Saved: mech_sprite_raw.bin" << std::endl;

    // Parse header
    if (data.size() < 11) {
        std::cerr << "File too small" << std::endl;
        return 1;
    }

    uint16_t type = (data[0] << 8) | data[1];
    uint16_t anim = (data[2] << 8) | data[3];
    uint16_t dim1 = (data[4] << 8) | data[5];
    // Byte 6 is padding
    std::string version(reinterpret_cast<char*>(&data[7]), 4);

    std::cout << "Type: " << type << ", Anim: " << anim << ", Dim: " << dim1 << std::endl;
    std::cout << "Version: " << version << std::endl;

    // Find dimensions from header
    int width = dim1;
    int height = dim1;
    if (width == 0 || width > 256) {
        width = 26; height = 26;  // fallback
    }

    std::cout << "Dimensions: " << width << "x" << height << std::endl;

    // Save raw pixels from different offsets as PGM images
    std::vector<size_t> offsets = {11, 50, 100, 150, 200, 250};
    for (size_t off : offsets) {
        if (off + width * height > data.size()) continue;

        std::string fname = "mech_at_" + std::to_string(off) + ".pgm";
        std::ofstream pgm(fname, std::ios::binary);
        pgm << "P5\n" << width << " " << height << "\n255\n";
        pgm.write(reinterpret_cast<char*>(&data[off]), width * height);
        pgm.close();

        // Analyze pattern
        int topQ = 0, midH = 0, botQ = 0;
        for (int row = 0; row < height; ++row) {
            int nz = 0;
            for (int col = 0; col < width; ++col) {
                if (data[off + row * width + col] != 0) nz++;
            }
            if (row < height / 4) topQ += nz;
            else if (row < 3 * height / 4) midH += nz;
            else botQ += nz;
        }
        std::cout << "Offset " << off << ": top=" << topQ << " mid=" << midH << " bot=" << botQ;
        if (midH > topQ && midH > botQ) std::cout << " [DIAMOND]";
        std::cout << std::endl;
    }

    // Also try VFX RLE decode from different offsets
    std::cout << "\nTrying VFX RLE decode..." << std::endl;
    for (size_t rleStart = 11; rleStart < 300; rleStart += 10) {
        std::vector<uint8_t> pixels(width * height, 0);
        const uint8_t* rle = &data[rleStart];
        size_t rleSize = data.size() - rleStart;
        size_t srcPos = 0;
        int x = 0, y = 0;
        bool valid = true;

        while (y < height && srcPos < rleSize && valid) {
            uint8_t marker = rle[srcPos++];

            if (marker == 0) {
                x = 0; y++;
            } else if (marker == 1) {
                if (srcPos >= rleSize) { valid = false; break; }
                int skip = rle[srcPos++];
                if (skip > width * 2) { valid = false; break; }
                x += skip;
            } else if ((marker & 1) == 0) {
                if (srcPos >= rleSize) { valid = false; break; }
                uint8_t color = rle[srcPos++];
                int count = marker >> 1;
                if (count > width) { valid = false; break; }
                for (int i = 0; i < count && x < width; ++i) {
                    if (y < height) pixels[y * width + x++] = color;
                }
            } else {
                int count = marker >> 1;
                if (count > width) { valid = false; break; }
                for (int i = 0; i < count && srcPos < rleSize && x < width; ++i) {
                    if (y < height) pixels[y * width + x++] = rle[srcPos++];
                }
            }
        }

        if (!valid || y < height / 2) continue;

        int nonZero = 0;
        for (uint8_t p : pixels) if (p != 0) nonZero++;

        int topQ = 0, midH = 0, botQ = 0;
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                if (pixels[row * width + col] != 0) {
                    if (row < height / 4) topQ++;
                    else if (row < 3 * height / 4) midH++;
                    else botQ++;
                }
            }
        }

        bool diamond = (midH > topQ + 10) && (midH > botQ + 10);
        if (nonZero > 100 && nonZero < width * height) {
            std::cout << "RLE@" << rleStart << ": " << nonZero << " px, top=" << topQ
                      << " mid=" << midH << " bot=" << botQ;
            if (diamond) {
                std::cout << " [DIAMOND] <-- LIKELY CORRECT";
                std::string fname = "mech_rle_" + std::to_string(rleStart) + ".pgm";
                std::ofstream pgm(fname, std::ios::binary);
                pgm << "P5\n" << width << " " << height << "\n255\n";
                pgm.write(reinterpret_cast<char*>(pixels.data()), pixels.size());
                std::cout << " -> " << fname;
            }
            std::cout << std::endl;
        }
    }

    return 0;
}
