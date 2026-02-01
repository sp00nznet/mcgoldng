# MechCommander Gold File Formats

This document describes the file formats used by MechCommander Gold (1998).

> **Note:** These formats differ from MechCommander 2! MCG uses an older, simpler format variant.

## Table of Contents

- [Archive Formats](#archive-formats)
  - [FST Archives](#fst-archives)
  - [PAK Archives](#pak-archives)
- [Compression](#compression)
  - [LZ Compression](#lz-compression)
  - [ZLIB Compression](#zlib-compression)
- [Configuration Files](#configuration-files)
  - [FIT Format](#fit-format)
- [Other Formats](#other-formats)

---

## Archive Formats

### FST Archives

FST (Fast Storage Table) archives are the main container format for game assets.

**Key Archives:**
| File | Contents |
|------|----------|
| `ART.FST` | GUI elements, menus, pilot portraits |
| `MISSION.FST` | Mission data, warrior profiles, ABL scripts |
| `MISC.FST` | Fonts, palettes, interface elements |
| `SHAPES.FST` | Sprite data |
| `TERRAIN.FST` | Map terrain data |

#### MCG FST Format (differs from MC2!)

```
┌─────────────────────────────────────┐
│ Header (4 bytes)                    │
├─────────────────────────────────────┤
│ Entry 0 (262 bytes)                 │
├─────────────────────────────────────┤
│ Entry 1 (262 bytes)                 │
├─────────────────────────────────────┤
│ ...                                 │
├─────────────────────────────────────┤
│ Entry N (262 bytes)                 │
├─────────────────────────────────────┤
│ File Data                           │
└─────────────────────────────────────┘
```

**Header:**
```
Offset  Size    Description
0       4       Entry count (DWORD, little-endian)
```

**Entry (262 bytes each):**
```
Offset  Size    Description
0       4       Data offset from start of file
4       4       Compressed size
8       4       Uncompressed size
12      250     File path (null-terminated, backslash separators)
```

**Compression Detection:**
- If `compressedSize < uncompressedSize`: Data is LZ compressed
- If `compressedSize == uncompressedSize`: Data is uncompressed

#### Code Example

```cpp
struct FstEntry {
    uint32_t offset;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    char     path[250];
};

// Read entries
uint32_t entryCount;
file.read(&entryCount, 4);

std::vector<FstEntry> entries(entryCount);
for (auto& entry : entries) {
    file.read(&entry, 262);
}
```

---

### PAK Archives

PAK (Packet) archives store multiple data packets, commonly used for sprites, sounds, and other grouped assets.

#### PAK Format

```
┌─────────────────────────────────────┐
│ Magic (4 bytes): 0xFEEDFACE         │
├─────────────────────────────────────┤
│ Seek Table (4 bytes per entry)      │
├─────────────────────────────────────┤
│ Packet 0 Data                       │
├─────────────────────────────────────┤
│ Packet 1 Data                       │
├─────────────────────────────────────┤
│ ...                                 │
└─────────────────────────────────────┘
```

**Magic Number:**
```
0xFEEDFACE (little-endian: CE FA ED FE)
```

**Seek Table Entry (4 bytes):**
```
Bits 0-28:  Packet data offset
Bits 29-31: Storage type
```

**Storage Types:**
| Value | Name | Description |
|-------|------|-------------|
| 0x00 | RAW | Uncompressed data |
| 0x01 | FWF | Fast Write Format |
| 0x02 | LZD | LZ compressed |
| 0x03 | HF | Huffman compressed |
| 0x04 | ZLIB | ZLIB compressed |
| 0x07 | NUL | Empty/null packet |

**Packet Count Calculation:**
```cpp
// First seek table entry points to first packet data
// Number of entries = (firstPacketOffset / 4) - 2
uint32_t firstOffset = seekTable[0] & 0x1FFFFFFF;
uint32_t packetCount = (firstOffset / 4) - 2;
```

**Compressed Packet Format:**
```
Offset  Size    Description
0       4       Uncompressed size (DWORD)
4       N       Compressed data
```

#### Code Example

```cpp
// Read and decode seek table entry
uint32_t raw;
file.read(&raw, 4);

uint32_t offset = raw & 0x1FFFFFFF;    // Lower 29 bits
uint8_t type = (raw >> 29) & 0x07;      // Upper 3 bits

// Read packet
file.seekg(offset);
if (type == STORAGE_LZD || type == STORAGE_ZLIB) {
    uint32_t uncompSize;
    file.read(&uncompSize, 4);
    // Read and decompress remaining data
}
```

---

## Compression

### LZ Compression

MCG uses a variable-bit LZW variant for compression.

#### Algorithm Constants

```cpp
static constexpr uint32_t HASH_CLEAR = 256;   // Clear hash table
static constexpr uint32_t HASH_EOF   = 257;   // End of data
static constexpr uint32_t HASH_FREE  = 258;   // First free code
static constexpr uint32_t BASE_BITS  = 9;     // Starting bit count
static constexpr uint32_t MAX_BITS   = 12;    // Maximum bit count
```

#### Decompression Algorithm

1. Initialize:
   - Bit count = 9
   - Free code = 258
   - Hash table empty

2. Read codes until EOF (257):
   - If code == 256 (CLEAR): Reset state, read next code as literal
   - If code < 258: Output literal byte
   - If code >= 258: Follow hash chain, output string

3. Add new entry to hash table:
   - Entry = (previous code, first byte of current output)
   - Increment free code
   - When free code reaches bit threshold, increase bit count

#### Hash Table Entry

```cpp
struct HashEntry {
    uint16_t chain;   // Previous code in chain
    uint8_t suffix;   // Character at this position
};
```

### ZLIB Compression

Standard ZLIB/deflate compression. Used in some PAK files.

```cpp
#include <zlib.h>

z_stream strm = {};
inflateInit(&strm);
strm.avail_in = compressedSize;
strm.next_in = compressedData;
strm.avail_out = uncompressedSize;
strm.next_out = outputBuffer;
inflate(&strm, Z_FINISH);
inflateEnd(&strm);
```

---

## Configuration Files

### FIT Format

FIT (FITini) files are text-based configuration files with typed variables.

#### Structure

```
FITini

[BlockName]
type variableName = value
type variableName = value

[AnotherBlock]
type variableName = value

FITend
```

#### Type Prefixes

| Prefix | Type | Example |
|--------|------|---------|
| `ul` | Unsigned long (32-bit) | `ul maxUnits = 12` |
| `l` | Signed long (32-bit) | `l temperature = -50` |
| `us` | Unsigned short (16-bit) | `us flags = 255` |
| `s` | Signed short (16-bit) | `s offset = -100` |
| `uc` | Unsigned char (8-bit) | `uc alpha = 128` |
| `c` | Signed char (8-bit) | `c direction = -1` |
| `f` | Float | `f speed = 3.14` |
| `b` | Boolean | `b enabled = TRUE` |
| `st` | String | `st name = "Atlas"` |

#### Arrays

Arrays use bracket notation after the type:

```
ul[4] coordinates = 100, 200, 300, 400
f[3] rgb = 1.0, 0.5, 0.0
```

#### Comments

Lines starting with `//` or `;` are comments:

```
// This is a comment
; This is also a comment
```

#### Example File

```
FITini

[SystemConfig]
ul screenWidth = 1920
ul screenHeight = 1080
b fullscreen = TRUE
f masterVolume = 0.8
st dataPath = "C:\Games\MCG"

[DebugSettings]
b showFPS = TRUE
b enableLogging = FALSE
ul logLevel = 2

FITend
```

#### Parsing Code

```cpp
FitParser parser;
if (parser.parseFile("config.fit")) {
    const FitBlock* system = parser.findBlock("SystemConfig");
    if (system) {
        auto width = system->getUInt("screenWidth");   // 1920
        auto fullscreen = system->getBool("fullscreen"); // true
        auto path = system->getString("dataPath");     // "C:\Games\MCG"
    }
}
```

---

## Other Formats

### Sprite Files (in PAK)

Sprite data is stored in PAK packets with indexed color (8-bit palette).

```
Header:
  - Width (2 bytes)
  - Height (2 bytes)
  - Hotspot X (2 bytes)
  - Hotspot Y (2 bytes)
  - Frame count (2 bytes)

Per Frame:
  - Frame offset table
  - Pixel data (palette indices)
```

### Terrain Files

Terrain data includes height maps and tile indices:

```
Map Header:
  - Width in tiles
  - Height in tiles
  - Tile size (45 or 90 pixels)

Per Tile:
  - Terrain type
  - Height value
  - Passability flags
```

### SMK Video

Smacker video format by RAD Game Tools. Contains pre-rendered cutscenes.

- 320x240 or 640x480 resolution
- Variable frame rate
- Embedded audio

Use libsmacker or FFmpeg for decoding.

### WAV Audio

Standard PCM WAV files for music and sound effects.

- 22050 Hz sample rate (common)
- 16-bit samples
- Mono or stereo

---

## Differences from MechCommander 2

| Feature | MechCommander Gold | MechCommander 2 |
|---------|-------------------|-----------------|
| FST Magic | None | `FSTF` header |
| FST Entry Size | 262 bytes | 264 bytes (includes hash) |
| FST Hash | None | 4-byte hash field |
| Compression | LZ, ZLIB | LZ, ZLIB, LZO |
| Config Format | FIT text | FIT text (expanded) |

---

## Tools and References

### Community Tools

- **SpinelDusk's FstFile/PakFile** - Python extraction scripts
- **MCGExtracted** - Pre-extracted asset collections

### Reference Source Code

The MechCommander 2 source code (released by Microsoft) provides reference implementations:

- `mclib/ffile.h` - FST format (MC2 variant)
- `mclib/packet.cpp` - PAK format
- `mclib/lzdecomp.cpp` - LZ decompression
- `mclib/inifile.cpp` - FIT parser
