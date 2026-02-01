#ifndef MCGNG_LZ_DECOMPRESS_H
#define MCGNG_LZ_DECOMPRESS_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace mcgng {

/**
 * LZ Decompression utilities for MechCommander Gold archives.
 *
 * MCG uses two compression schemes:
 * 1. LZD - Custom LZ variant (variable-bit codes, hash table based)
 * 2. ZLIB - Standard zlib compression
 *
 * This implementation provides both decompressors.
 */

/**
 * Decompress LZ-compressed data using the custom MCG/MC2 LZD algorithm.
 * This is a portable C++ reimplementation of the original x86 assembly.
 *
 * @param src       Pointer to compressed data
 * @param srcLen    Length of compressed data in bytes
 * @param dest      Pointer to output buffer (must be pre-allocated)
 * @param destLen   Size of output buffer
 * @return          Number of bytes decompressed, or 0 on error
 */
size_t lzDecompress(const uint8_t* src, size_t srcLen, uint8_t* dest, size_t destLen);

/**
 * Decompress ZLIB-compressed data.
 *
 * @param src       Pointer to compressed data
 * @param srcLen    Length of compressed data in bytes
 * @param dest      Pointer to output buffer (must be pre-allocated)
 * @param destLen   Size of output buffer (expected uncompressed size)
 * @return          Number of bytes decompressed, or 0 on error
 */
size_t zlibDecompress(const uint8_t* src, size_t srcLen, uint8_t* dest, size_t destLen);

/**
 * Convenience function that returns decompressed data as a vector.
 *
 * @param src               Pointer to compressed data
 * @param srcLen            Length of compressed data
 * @param uncompressedSize  Expected uncompressed size
 * @param useZlib           If true, use zlib; if false, use LZD
 * @return                  Decompressed data, or empty vector on error
 */
std::vector<uint8_t> decompress(const uint8_t* src, size_t srcLen,
                                 size_t uncompressedSize, bool useZlib);

} // namespace mcgng

#endif // MCGNG_LZ_DECOMPRESS_H
