#include "assets/tga_loader.h"
#include <fstream>
#include <cstring>
#include <iostream>

namespace mcgng {

// TGA header structure
#pragma pack(push, 1)
struct TgaHeader {
    uint8_t idLength;           // Length of image ID field
    uint8_t colorMapType;       // 0 = no color map, 1 = has color map
    uint8_t imageType;          // Image type (see below)
    // Color map specification (5 bytes)
    uint16_t colorMapOrigin;    // First entry index
    uint16_t colorMapLength;    // Number of entries
    uint8_t colorMapDepth;      // Bits per entry
    // Image specification (10 bytes)
    uint16_t xOrigin;           // X origin
    uint16_t yOrigin;           // Y origin
    uint16_t width;             // Image width
    uint16_t height;            // Image height
    uint8_t pixelDepth;         // Bits per pixel
    uint8_t imageDescriptor;    // Image descriptor (alpha bits, origin)
};
#pragma pack(pop)

// Image types
enum TgaImageType {
    TGA_NO_IMAGE = 0,
    TGA_COLORMAPPED = 1,
    TGA_TRUECOLOR = 2,
    TGA_GRAYSCALE = 3,
    TGA_COLORMAPPED_RLE = 9,
    TGA_TRUECOLOR_RLE = 10,
    TGA_GRAYSCALE_RLE = 11
};

TgaImage TgaLoader::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "TgaLoader: Failed to open file: " << path << "\n";
        return TgaImage{};
    }

    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "TgaLoader: Failed to read file: " << path << "\n";
        return TgaImage{};
    }

    return loadFromMemory(data.data(), data.size());
}

TgaImage TgaLoader::loadFromMemory(const uint8_t* data, size_t size) {
    TgaImage result;

    if (!data || size < sizeof(TgaHeader)) {
        std::cerr << "TgaLoader: Invalid data or size\n";
        return result;
    }

    // Read header
    TgaHeader header;
    std::memcpy(&header, data, sizeof(TgaHeader));

    result.width = header.width;
    result.height = header.height;
    result.bitsPerPixel = header.pixelDepth;
    result.hasAlpha = (header.imageDescriptor & 0x0F) > 0;

    if (result.width <= 0 || result.height <= 0 || result.width > 8192 || result.height > 8192) {
        std::cerr << "TgaLoader: Invalid dimensions: " << result.width << "x" << result.height << "\n";
        return TgaImage{};
    }

    // Calculate data offset
    size_t dataOffset = sizeof(TgaHeader) + header.idLength;

    // Read color map if present
    std::vector<uint8_t> colorMap;
    if (header.colorMapType == 1 && header.colorMapLength > 0) {
        size_t colorMapSize = header.colorMapLength * (header.colorMapDepth / 8);
        if (dataOffset + colorMapSize > size) {
            std::cerr << "TgaLoader: Color map extends past end of data\n";
            return TgaImage{};
        }
        colorMap.resize(colorMapSize);
        std::memcpy(colorMap.data(), data + dataOffset, colorMapSize);
        dataOffset += colorMapSize;
    }

    // Allocate RGBA output buffer
    result.pixels.resize(result.width * result.height * 4);

    const uint8_t* pixelData = data + dataOffset;
    size_t pixelDataSize = size - dataOffset;

    bool isRLE = (header.imageType == TGA_COLORMAPPED_RLE ||
                  header.imageType == TGA_TRUECOLOR_RLE ||
                  header.imageType == TGA_GRAYSCALE_RLE);

    bool isColorMapped = (header.imageType == TGA_COLORMAPPED ||
                          header.imageType == TGA_COLORMAPPED_RLE);

    bool isGrayscale = (header.imageType == TGA_GRAYSCALE ||
                        header.imageType == TGA_GRAYSCALE_RLE);

    int bytesPerPixel = header.pixelDepth / 8;
    int totalPixels = result.width * result.height;

    // Decode pixels
    size_t srcPos = 0;
    int pixelIndex = 0;

    while (pixelIndex < totalPixels && srcPos < pixelDataSize) {
        int count = 1;
        bool isRunPacket = false;

        if (isRLE) {
            uint8_t packetHeader = pixelData[srcPos++];
            isRunPacket = (packetHeader & 0x80) != 0;
            count = (packetHeader & 0x7F) + 1;
        }

        for (int i = 0; i < count && pixelIndex < totalPixels; ++i) {
            uint8_t r = 0, g = 0, b = 0, a = 255;

            // Read pixel value (only once for run packets)
            if (!isRLE || !isRunPacket || i == 0) {
                if (srcPos >= pixelDataSize) break;

                if (isColorMapped) {
                    // Color-mapped: pixel is index into color map
                    uint8_t index = pixelData[srcPos++];
                    int cmBpp = header.colorMapDepth / 8;
                    int cmIndex = (index - header.colorMapOrigin) * cmBpp;

                    if (cmIndex >= 0 && cmIndex + cmBpp <= static_cast<int>(colorMap.size())) {
                        if (cmBpp >= 3) {
                            b = colorMap[cmIndex];
                            g = colorMap[cmIndex + 1];
                            r = colorMap[cmIndex + 2];
                            if (cmBpp >= 4) a = colorMap[cmIndex + 3];
                        }
                    }
                } else if (isGrayscale) {
                    // Grayscale
                    uint8_t gray = pixelData[srcPos++];
                    r = g = b = gray;
                    if (bytesPerPixel >= 2) {
                        a = pixelData[srcPos++];
                    }
                } else {
                    // True color (BGR or BGRA)
                    if (bytesPerPixel >= 3 && srcPos + 2 < pixelDataSize) {
                        b = pixelData[srcPos++];
                        g = pixelData[srcPos++];
                        r = pixelData[srcPos++];
                    }
                    if (bytesPerPixel >= 4 && srcPos < pixelDataSize) {
                        a = pixelData[srcPos++];
                    }
                    if (bytesPerPixel == 2 && srcPos + 1 < pixelDataSize) {
                        // 16-bit (ARRRRRGG GGGBBBBB or similar)
                        uint16_t pixel = pixelData[srcPos] | (pixelData[srcPos + 1] << 8);
                        srcPos += 2;
                        b = (pixel & 0x1F) << 3;
                        g = ((pixel >> 5) & 0x1F) << 3;
                        r = ((pixel >> 10) & 0x1F) << 3;
                        a = (pixel & 0x8000) ? 255 : 0;
                    }
                }
            }

            // Write RGBA pixel
            int outIndex = pixelIndex * 4;
            result.pixels[outIndex + 0] = r;
            result.pixels[outIndex + 1] = g;
            result.pixels[outIndex + 2] = b;
            result.pixels[outIndex + 3] = a;
            pixelIndex++;
        }
    }

    // Handle origin (TGA can be top-left or bottom-left origin)
    bool topOrigin = (header.imageDescriptor & 0x20) != 0;
    if (!topOrigin) {
        // Flip vertically (TGA is bottom-to-top by default)
        int rowSize = result.width * 4;
        std::vector<uint8_t> tempRow(rowSize);
        for (int y = 0; y < result.height / 2; ++y) {
            int topRow = y * rowSize;
            int bottomRow = (result.height - 1 - y) * rowSize;
            std::memcpy(tempRow.data(), &result.pixels[topRow], rowSize);
            std::memcpy(&result.pixels[topRow], &result.pixels[bottomRow], rowSize);
            std::memcpy(&result.pixels[bottomRow], tempRow.data(), rowSize);
        }
    }

    return result;
}

} // namespace mcgng
