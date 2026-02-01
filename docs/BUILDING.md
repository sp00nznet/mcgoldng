# Building MCG-NG

This guide covers building MCG-NG from source on Windows.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Quick Build (Tools Only)](#quick-build-tools-only)
- [Full Build (With Game)](#full-build-with-game)
- [Build Options](#build-options)
- [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Required

| Tool | Version | Download |
|------|---------|----------|
| Visual Studio | 2022 | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| CMake | 3.16+ | [cmake.org](https://cmake.org/download/) |
| Git | Any | [git-scm.com](https://git-scm.com/) |

### Optional (for full game build)

| Library | Purpose | Installation |
|---------|---------|--------------|
| SDL2 | Graphics & Input | vcpkg or manual |
| SDL2_mixer | Audio | vcpkg or manual |
| GLEW | OpenGL Extensions | vcpkg or manual |
| zlib | ZLIB Decompression | vcpkg or manual |

---

## Quick Build (Tools Only)

This builds just the extraction tool without any external dependencies.

### Step 1: Clone the Repository

```bash
git clone https://github.com/sp00nznet/mcgoldng.git
cd mcgoldng
```

### Step 2: Configure with CMake

```bash
cmake -B build
```

You should see output like:
```
-- ZLIB not found - using built-in LZ decompression only
-- SDL2, OpenGL, or GLEW not found - building tools only
-- Configuring done
-- Generating done
```

### Step 3: Build

```bash
cmake --build build
```

### Step 4: Find the Executable

The extraction tool will be at:
```
build/Debug/mcg-extract.exe
```

---

## Full Build (With Game)

To build the full game with graphics, audio, and video support.

### Using vcpkg (Recommended)

#### Step 1: Install vcpkg

```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat
```

#### Step 2: Install Dependencies

```bash
vcpkg install sdl2:x64-windows
vcpkg install sdl2-mixer:x64-windows
vcpkg install glew:x64-windows
vcpkg install zlib:x64-windows
```

#### Step 3: Configure with vcpkg Toolchain

```bash
cd mcgoldng
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

#### Step 4: Build

```bash
cmake --build build --config Release
```

### Manual Installation

If you prefer to install libraries manually:

1. Download SDL2 development libraries from [libsdl.org](https://libsdl.org/)
2. Download SDL2_mixer from [SDL_mixer](https://github.com/libsdl-org/SDL_mixer)
3. Download GLEW from [glew.sourceforge.net](http://glew.sourceforge.net/)
4. Set environment variables or CMake cache entries pointing to each library

---

## Build Options

CMake options can be set with `-D<OPTION>=<VALUE>`:

| Option | Default | Description |
|--------|---------|-------------|
| `MCGNG_BUILD_TOOLS` | ON | Build extraction tools |
| `MCGNG_BUILD_TESTS` | OFF | Build unit tests |
| `MCGNG_USE_SYSTEM_ZLIB` | ON | Use system zlib if available |

### Examples

```bash
# Build without tools
cmake -B build -DMCGNG_BUILD_TOOLS=OFF

# Build with tests
cmake -B build -DMCGNG_BUILD_TESTS=ON

# Disable zlib (use only built-in LZ)
cmake -B build -DMCGNG_USE_SYSTEM_ZLIB=OFF
```

---

## Build Configurations

### Debug (Default)

```bash
cmake --build build --config Debug
```

- Includes debug symbols
- No optimizations
- Assertions enabled

### Release

```bash
cmake --build build --config Release
```

- Full optimizations
- No debug symbols
- Assertions disabled

### RelWithDebInfo

```bash
cmake --build build --config RelWithDebInfo
```

- Optimizations enabled
- Debug symbols included
- Good for profiling

---

## IDE Integration

### Visual Studio

```bash
cmake -B build -G "Visual Studio 17 2022"
```

Then open `build/mcgoldng.sln` in Visual Studio.

### Visual Studio Code

1. Install the CMake Tools extension
2. Open the mcgoldng folder
3. Press `Ctrl+Shift+P` and select "CMake: Configure"
4. Press `F7` to build

---

## Troubleshooting

### CMake can't find SDL2

**Solution:** Install via vcpkg or set `SDL2_DIR`:

```bash
cmake -B build -DSDL2_DIR=C:/path/to/SDL2/cmake
```

### ZLIB not found

**Not a problem!** MCG-NG has a built-in LZ decompressor. ZLIB is optional and only needed for ZLIB-compressed assets (rare in MCG).

### Build fails with C++ standard errors

**Solution:** Ensure you have Visual Studio 2019 or later with C++17 support.

### "Cannot open include file" errors

**Solution:** Run CMake configure again to regenerate the build files:

```bash
cmake -B build --fresh
```

---

## Output Files

After a successful build:

| File | Location | Description |
|------|----------|-------------|
| `mcg-extract.exe` | `build/Debug/` | Asset extraction tool |
| `mcgoldng.exe` | `build/Debug/` | Main game (if SDL2 available) |
| `mcgng_assets.lib` | `build/Debug/` | Asset library |
| `mcgng_core.lib` | `build/Debug/` | Core engine library |

---

## Next Steps

After building:

1. Run `mcg-extract.exe` to extract game assets
2. See [Usage](../README.md#usage) for running the game
3. See [Architecture](ARCHITECTURE.md) for code overview
