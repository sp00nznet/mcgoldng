# MechCommander Gold: Next Generation

<p align="center">
  <img src="docs/images/mcgng-banner.png" alt="MCG-NG Banner" width="600">
</p>

<p align="center">
  <strong>A modern, open-source reimplementation of MechCommander Gold (1998)</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#building">Building</a> •
  <a href="#usage">Usage</a> •
  <a href="#roadmap">Roadmap</a> •
  <a href="#contributing">Contributing</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-Windows%2011+-blue?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/C++-17-orange?style=flat-square&logo=cplusplus" alt="C++17">
  <img src="https://img.shields.io/badge/License-MIT-green?style=flat-square" alt="License">
  <img src="https://img.shields.io/badge/Status-In%20Development-yellow?style=flat-square" alt="Status">
</p>

---

## About

**MCG-NG** is a "Bring Your Own Game" reimplementation of the classic 1998 real-time tactics game *MechCommander Gold*. It reads assets directly from your legal copy of the game and runs them in a modern engine built with SDL2 and OpenGL.

> **Note:** This project does not include any copyrighted game assets. You must own a copy of MechCommander Gold to use this software.

---

## Features

### Current Implementation Status

| Component | Status | Description |
|-----------|--------|-------------|
| **Asset Extraction** | | |
| FST Archive Reader | ✅ Complete | Reads MCG's main archive format |
| PAK Archive Reader | ✅ Complete | Reads packet-based archives |
| LZ Decompression | ✅ Complete | Native LZ decoder (no zlib required) |
| ZLIB Decompression | ✅ Complete | Optional zlib support |
| FIT Config Parser | ✅ Complete | Parses game configuration files |
| Extraction Tool | ✅ Complete | CLI tool to extract all game assets |
| **Core Systems** | | |
| Engine Framework | ✅ Complete | Main game loop and state management |
| Configuration | ✅ Complete | Settings and preferences system |
| Memory Management | ✅ Complete | Pool allocators and tracking |
| **Graphics** | | |
| Renderer (SDL2/OpenGL) | 🔶 Stub | Window and rendering context |
| Sprite System | 🔶 Stub | Sprite loading and animation |
| Terrain Renderer | 🔶 Stub | Isometric tile rendering |
| UI System | 🔶 Stub | Interface elements |
| **Audio** | | |
| Audio System | 🔶 Stub | SDL2_mixer integration |
| Music Manager | 🔶 Stub | Background music playback |
| **Video** | | |
| SMK Player | 🔶 Stub | Smacker video playback |
| **Game Logic** | | |
| Mech System | ✅ Complete | Mech components, weapons, database |
| Combat System | ✅ Complete | Damage, projectiles, hit resolution |
| Mission System | ✅ Complete | Objectives, triggers, spawns |
| ABL Scripting | ❌ Not Started | Mission scripting language |

**Legend:** ✅ Complete | 🔶 Stub/Partial | ❌ Not Started

---

## Installation

### Requirements

- **Windows 11** or later
- **MechCommander Gold** game files (ISO or installed copy)
- **Visual Studio 2022** (for building)
- **CMake 3.16+**

### Optional Dependencies

- **SDL2** - For graphics/input (game executable)
- **SDL2_mixer** - For audio (game executable)
- **OpenGL/GLEW** - For rendering (game executable)
- **zlib** - For ZLIB-compressed assets (optional, has fallback)

---

## Building

### Quick Start

```bash
# Clone the repository
git clone https://github.com/sp00nznet/mcgoldng.git
cd mcgoldng

# Configure (tools only - no SDL2 required)
cmake -B build

# Build
cmake --build build

# The extraction tool will be at: build/Debug/mcg-extract.exe
```

### Full Build (with Game)

```bash
# Install dependencies via vcpkg
vcpkg install sdl2 sdl2-mixer glew zlib

# Configure with vcpkg toolchain
cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build
```

See [docs/BUILDING.md](docs/BUILDING.md) for detailed build instructions.

---

## Usage

### Extracting Game Assets

```bash
# Extract all assets from game folder
mcg-extract.exe "C:\Games\MechCommander Gold" "C:\MCG-Extracted"

# Or from an ISO mounted as D:
mcg-extract.exe "D:\" "C:\MCG-Extracted"
```

### Running the Game

```bash
# Run with extracted assets
mcgoldng.exe --data "C:\MCG-Extracted"

# Or point directly to game folder
mcgoldng.exe --data "C:\Games\MechCommander Gold"
```

---

## Project Structure

```
mcgoldng/
├── CMakeLists.txt          # Build configuration
├── cmake/                  # CMake modules
│   └── FindSDL2_mixer.cmake
├── src/
│   ├── main.cpp            # Entry point
│   ├── assets/             # Asset loading
│   │   ├── fst_reader.*    # FST archive format
│   │   ├── pak_reader.*    # PAK archive format
│   │   ├── fit_parser.*    # FIT config format
│   │   └── lz_decompress.* # Decompression
│   ├── core/               # Engine core
│   │   ├── engine.*        # Game loop
│   │   ├── config.*        # Settings
│   │   └── memory.*        # Memory management
│   ├── graphics/           # Rendering
│   ├── audio/              # Sound
│   ├── video/              # Video playback
│   └── game/               # Game logic
│       ├── mech.*          # Mech units
│       ├── combat.*        # Combat system
│       └── mission.*       # Missions
├── tools/
│   └── mcg_extract.cpp     # Asset extraction tool
└── docs/                   # Documentation
```

---

## Roadmap

### Phase 1: Asset Pipeline ✅
- [x] FST archive reader
- [x] PAK archive reader
- [x] LZ/ZLIB decompression
- [x] FIT config parser
- [x] Extraction CLI tool

### Phase 2: Core Engine 🔶
- [x] Engine framework
- [x] Configuration system
- [x] Memory management
- [ ] Resource manager with caching

### Phase 3: Graphics
- [ ] SDL2 window creation
- [ ] OpenGL context setup
- [ ] Sprite loading from PAK
- [ ] Palette handling (8-bit indexed)
- [ ] Isometric terrain rendering
- [ ] UI rendering

### Phase 4: Audio & Video
- [ ] SDL2_mixer audio playback
- [ ] Music streaming
- [ ] Sound effects
- [ ] SMK video decoder

### Phase 5: Game Logic
- [x] Mech data structures
- [x] Combat system
- [x] Mission system
- [ ] ABL script interpreter
- [ ] AI behavior
- [ ] Pathfinding

### Phase 6: Polish
- [ ] Save/load system
- [ ] Full campaign support
- [ ] Multiplayer (stretch goal)
- [ ] Mod support

---

## Documentation

| Document | Description |
|----------|-------------|
| [Building](docs/BUILDING.md) | Detailed build instructions |
| [Architecture](docs/ARCHITECTURE.md) | Code organization and design |
| [File Formats](docs/FILE_FORMATS.md) | MCG file format specifications |
| [Contributing](docs/CONTRIBUTING.md) | How to contribute |

---

## Contributing

Contributions are welcome! Please read our [Contributing Guide](docs/CONTRIBUTING.md) before submitting PRs.

### Areas Needing Help

- **Graphics Programming** - SDL2/OpenGL rendering implementation
- **Audio Engineering** - Sound system implementation
- **Reverse Engineering** - Documenting undocumented file formats
- **Testing** - Testing with different game versions

---

## Acknowledgments

- **FASA Interactive** - Original MechCommander developers
- **Microsoft** - MechCommander 2 source code release
- **SpinelDusk** - FstFile/PakFile format research
- **ModDB Community** - Documentation and modding resources

---

## Legal

This project is not affiliated with Microsoft, FASA Interactive, or any rights holders of the MechCommander franchise.

MechCommander is a trademark of Microsoft Corporation. This project is a clean-room reimplementation that requires users to provide their own legally obtained copy of the game.

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

<p align="center">
  <i>For the glory of House Steiner!</i>
</p>
