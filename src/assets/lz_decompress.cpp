#include "assets/lz_decompress.h"
#include <cstring>
#include <stdexcept>

#ifdef MCGNG_HAS_ZLIB
#include <zlib.h>
#endif

namespace mcgng {

// LZ decompression constants (from original MC2 source)
static constexpr uint32_t HASH_CLEAR = 256;   // Clear hash table command code
static constexpr uint32_t HASH_EOF = 257;     // End Of Data command code
static constexpr uint32_t HASH_FREE = 258;    // First Hash Table Chain Offset Value
static constexpr uint32_t BASE_BITS = 9;
static constexpr uint32_t MAX_BIT_INDEX = (1 << BASE_BITS);
static constexpr size_t HASH_TABLE_SIZE = 16384;

// Hash table entry structure
struct HashEntry {
    uint16_t chain;
    uint8_t suffix;
};

/**
 * Portable C++ implementation of the LZ decompression algorithm.
 * This reimplements the x86 assembly from the original MC2 source.
 */
size_t lzDecompress(const uint8_t* src, size_t srcLen, uint8_t* dest, size_t destLen) {
    if (!src || !dest || srcLen < 3 || destLen == 0) {
        return 0;
    }

    // Hash table for decompression
    HashEntry hashTable[HASH_TABLE_SIZE / 3];
    std::memset(hashTable, 0, sizeof(hashTable));

    // State variables
    uint32_t codeMask = MAX_BIT_INDEX - 1;
    uint32_t maxIndex = MAX_BIT_INDEX;
    uint32_t freeIndex = HASH_FREE;
    uint32_t oldChain = 0;
    uint8_t oldSuffix = 0;
    uint32_t bitCount = BASE_BITS;

    // Bit reading state
    size_t srcPos = 0;
    uint32_t bitBuffer = 0;
    uint32_t bitsInBuffer = 0;

    // Output position
    size_t destPos = 0;

    // Stack for outputting characters in reverse order
    uint8_t charStack[4096];
    size_t stackPos = 0;

    // Helper lambda to read bits from the source
    auto readCode = [&]() -> uint32_t {
        // Ensure we have enough bits
        while (bitsInBuffer < bitCount && srcPos < srcLen) {
            bitBuffer |= static_cast<uint32_t>(src[srcPos++]) << bitsInBuffer;
            bitsInBuffer += 8;
        }

        uint32_t code = bitBuffer & codeMask;
        bitBuffer >>= bitCount;
        bitsInBuffer -= bitCount;
        return code;
    };

    // Read first code after a clear
    auto readFirstCode = [&]() -> bool {
        if (srcPos > srcLen - 3) {
            return false;
        }

        uint32_t code = readCode();
        if (code == HASH_EOF) {
            return false;
        }

        oldChain = code;
        oldSuffix = static_cast<uint8_t>(code);

        if (destPos < destLen) {
            dest[destPos++] = oldSuffix;
        }
        return true;
    };

    // Initial read
    if (!readFirstCode()) {
        return destPos;
    }

    // Main decompression loop
    while (srcPos <= srcLen) {
        uint32_t code = readCode();

        // Check for special codes
        if (code == HASH_EOF) {
            break;
        }

        if (code == HASH_CLEAR) {
            // Reset state
            bitCount = BASE_BITS;
            codeMask = MAX_BIT_INDEX - 1;
            maxIndex = MAX_BIT_INDEX;
            freeIndex = HASH_FREE;

            if (!readFirstCode()) {
                break;
            }
            continue;
        }

        // Save current code for later
        uint32_t newChain = code;
        stackPos = 0;

        // Handle the case where code is not in table yet
        if (code >= freeIndex) {
            charStack[stackPos++] = oldSuffix;
            code = oldChain;
        }

        // Follow the chain to build output (in reverse)
        while (code >= HASH_FREE) {
            if (stackPos >= sizeof(charStack)) {
                return 0; // Stack overflow - corrupted data
            }
            charStack[stackPos++] = hashTable[code - HASH_FREE].suffix;
            code = hashTable[code - HASH_FREE].chain;
        }

        // Output the final character
        oldSuffix = static_cast<uint8_t>(code);
        if (destPos < destLen) {
            dest[destPos++] = oldSuffix;
        }

        // Output stacked characters in reverse order
        while (stackPos > 0) {
            if (destPos < destLen) {
                dest[destPos++] = charStack[--stackPos];
            } else {
                --stackPos;
            }
        }

        // Add new entry to hash table
        if (freeIndex < HASH_TABLE_SIZE / 3 + HASH_FREE) {
            hashTable[freeIndex - HASH_FREE].chain = static_cast<uint16_t>(oldChain);
            hashTable[freeIndex - HASH_FREE].suffix = oldSuffix;
            freeIndex++;

            // Check if we need to increase bit count
            if (freeIndex >= maxIndex && bitCount < 12) {
                bitCount++;
                maxIndex <<= 1;
                codeMask = (codeMask << 1) | 1;
            }
        }

        oldChain = newChain;
    }

    return destPos;
}

size_t zlibDecompress(const uint8_t* src, size_t srcLen, uint8_t* dest, size_t destLen) {
    if (!src || !dest || srcLen == 0 || destLen == 0) {
        return 0;
    }

#ifdef MCGNG_HAS_ZLIB
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    int ret = inflateInit(&strm);
    if (ret != Z_OK) {
        return 0;
    }

    strm.avail_in = static_cast<uInt>(srcLen);
    strm.next_in = const_cast<Bytef*>(src);
    strm.avail_out = static_cast<uInt>(destLen);
    strm.next_out = dest;

    ret = inflate(&strm, Z_FINISH);
    size_t outSize = destLen - strm.avail_out;

    inflateEnd(&strm);

    if (ret != Z_STREAM_END && ret != Z_OK) {
        return 0;
    }

    return outSize;
#else
    // ZLIB not available - cannot decompress ZLIB-compressed data
    (void)src;
    (void)srcLen;
    (void)dest;
    (void)destLen;
    return 0;
#endif
}

std::vector<uint8_t> decompress(const uint8_t* src, size_t srcLen,
                                 size_t uncompressedSize, bool useZlib) {
    if (!src || srcLen == 0 || uncompressedSize == 0) {
        return {};
    }

    std::vector<uint8_t> result(uncompressedSize);
    size_t actualSize;

    if (useZlib) {
        actualSize = zlibDecompress(src, srcLen, result.data(), uncompressedSize);
    } else {
        // Try LZW first
        actualSize = lzDecompress(src, srcLen, result.data(), uncompressedSize);

        // If LZW failed or returned very little data, try zlib as fallback
        // MCG may use different compression than MC2
        if (actualSize < uncompressedSize / 2) {
            size_t zlibSize = zlibDecompress(src, srcLen, result.data(), uncompressedSize);
            if (zlibSize > actualSize) {
                actualSize = zlibSize;
            }
        }
    }

    if (actualSize == 0) {
        return {};
    }

    result.resize(actualSize);
    return result;
}

} // namespace mcgng
