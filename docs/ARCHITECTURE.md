# Architecture Overview

This document describes the high-level architecture of MCG-NG.

## Table of Contents

- [Design Philosophy](#design-philosophy)
- [Module Overview](#module-overview)
- [Layer Diagram](#layer-diagram)
- [Key Components](#key-components)
- [Data Flow](#data-flow)
- [Threading Model](#threading-model)

---

## Design Philosophy

MCG-NG follows these principles:

1. **Modularity** - Each system is self-contained with clear interfaces
2. **Portability** - Minimal platform-specific code (Windows focus, but portable design)
3. **Testability** - Systems can be tested in isolation
4. **Original Compatibility** - Behavior matches original game where possible

---

## Module Overview

```
┌─────────────────────────────────────────────────────────────┐
│                        Application                          │
│                        (main.cpp)                           │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        ▼                     ▼                     ▼
┌───────────────┐    ┌───────────────┐    ┌───────────────┐
│   Graphics    │    │     Audio     │    │     Video     │
│   (SDL2/GL)   │    │  (SDL2_mixer) │    │  (SMK Player) │
└───────────────┘    └───────────────┘    └───────────────┘
        │                     │                     │
        └─────────────────────┼─────────────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Game Logic                             │
│              (mech, combat, mission, abl)                   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                        Core                                 │
│              (engine, config, memory)                       │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                       Assets                                │
│         (fst_reader, pak_reader, fit_parser, lz)            │
└─────────────────────────────────────────────────────────────┘
```

---

## Layer Diagram

### Assets Layer (`src/assets/`)

The foundation layer that reads original game files.

| Component | File | Purpose |
|-----------|------|---------|
| **FstReader** | `fst_reader.h/cpp` | Reads FST archive files |
| **PakReader** | `pak_reader.h/cpp` | Reads PAK packet archives |
| **FitParser** | `fit_parser.h/cpp` | Parses FIT configuration files |
| **LZ Decompress** | `lz_decompress.h/cpp` | Decompresses LZ/ZLIB data |

**Key Classes:**

```cpp
class FstReader {
    bool open(const std::string& path);
    std::vector<uint8_t> readFile(const std::string& name);
    const std::vector<FstEntry>& getEntries() const;
};

class PakReader {
    bool open(const std::string& path);
    std::vector<uint8_t> readPacket(uint32_t index);
    uint32_t getPacketCount() const;
};

class FitParser {
    bool parseFile(const std::string& path);
    const FitBlock* findBlock(const std::string& name) const;
};
```

### Core Layer (`src/core/`)

Fundamental engine systems.

| Component | File | Purpose |
|-----------|------|---------|
| **Engine** | `engine.h/cpp` | Main loop, state management |
| **Config** | `config.h/cpp` | Settings persistence |
| **Memory** | `memory.h/cpp` | Pool allocators, tracking |

**Engine States:**

```cpp
enum class GameState {
    Startup,      // Initial loading
    MainMenu,     // Main menu
    Loading,      // Loading mission
    InGame,       // Gameplay
    Paused,       // Paused
    Cutscene,     // Video playback
    Shutdown      // Exiting
};
```

### Game Layer (`src/game/`)

Game logic and simulation.

| Component | File | Purpose |
|-----------|------|---------|
| **Mech** | `mech.h/cpp` | Mech units, components, weapons |
| **Combat** | `combat.h/cpp` | Damage, projectiles, hits |
| **Mission** | `mission.h/cpp` | Objectives, triggers, spawns |

**Mech Component Model:**

```cpp
enum class MechLocation {
    Head, CenterTorso, LeftTorso, RightTorso,
    LeftArm, RightArm, LeftLeg, RightLeg
};

struct MechComponent {
    int armor;
    int internalStructure;
    bool destroyed;
    std::vector<std::string> equipment;
};
```

### Graphics Layer (`src/graphics/`)

Rendering and display.

| Component | File | Purpose |
|-----------|------|---------|
| **Renderer** | `renderer.h/cpp` | SDL2 window, OpenGL context |
| **Sprite** | `sprite.h/cpp` | Sprite sheets, animations |
| **Terrain** | `terrain.h/cpp` | Isometric tile rendering |
| **UI** | `ui.h/cpp` | Interface elements |

### Audio Layer (`src/audio/`)

Sound and music.

| Component | File | Purpose |
|-----------|------|---------|
| **AudioSystem** | `audio_system.h/cpp` | SDL2_mixer, sound effects |
| **MusicManager** | `music_manager.h/cpp` | Background music, crossfade |

### Video Layer (`src/video/`)

Video playback for cutscenes.

| Component | File | Purpose |
|-----------|------|---------|
| **SmkPlayer** | `smk_player.h/cpp` | Smacker video decoder |

---

## Key Components

### Singleton Pattern

Several managers use the singleton pattern for global access:

```cpp
// Access patterns
Engine& engine = Engine::instance();
CombatSystem& combat = CombatSystem::instance();
WeaponDatabase& weapons = WeaponDatabase::instance();
```

### Memory Management

Custom allocators for performance-critical systems:

```cpp
class MemoryPool {
    void* allocate(size_t size);
    void deallocate(void* ptr);
    void reset();
};

class MemoryTracker {
    void trackAllocation(void* ptr, size_t size, const char* file, int line);
    void trackDeallocation(void* ptr);
    void reportLeaks();
};
```

### Event System

Combat events use a callback pattern:

```cpp
using CombatEventCallback = std::function<void(const CombatEvent&)>;

// Register callback
combat.setEventCallback([](const CombatEvent& event) {
    switch (event.type) {
        case CombatEventType::Hit:
            // Handle hit
            break;
        case CombatEventType::MechDestroyed:
            // Handle destruction
            break;
    }
});
```

---

## Data Flow

### Asset Loading Flow

```
Game Folder
    │
    ▼
┌─────────────┐
│ FstReader   │──▶ Reads FST archives (ART, MISSION, MISC, etc.)
└─────────────┘
    │
    ▼
┌─────────────┐
│ PakReader   │──▶ Reads PAK files within FST or standalone
└─────────────┘
    │
    ▼
┌─────────────┐
│ LZ Decomp   │──▶ Decompresses data if needed
└─────────────┘
    │
    ▼
┌─────────────┐
│ Format      │──▶ Sprite, Terrain, Mission parsers
│ Parsers     │
└─────────────┘
    │
    ▼
  Runtime Objects (Sprites, Maps, Missions)
```

### Game Loop Flow

```
┌──────────────────────────────────────────┐
│               Main Loop                   │
│                                          │
│  ┌────────┐  ┌────────┐  ┌────────┐     │
│  │ Input  │─▶│ Update │─▶│ Render │     │
│  └────────┘  └────────┘  └────────┘     │
│       │           │           │          │
│       ▼           ▼           ▼          │
│  Process     Physics      Draw           │
│  Events      Combat       Sprites        │
│  Commands    AI           Terrain        │
│              Movement     UI             │
│                                          │
└──────────────────────────────────────────┘
```

### Combat Flow

```
Weapon Fired
    │
    ▼
┌─────────────────┐
│ Range Check     │──▶ Out of range? Fail
└─────────────────┘
    │
    ▼
┌─────────────────┐
│ Create          │──▶ Projectile or instant hit
│ Projectile      │
└─────────────────┘
    │
    ▼
┌─────────────────┐
│ Travel Time     │──▶ (Ballistic weapons only)
└─────────────────┘
    │
    ▼
┌─────────────────┐
│ Hit Chance      │──▶ Miss? Fire Miss event
│ Roll            │
└─────────────────┘
    │
    ▼
┌─────────────────┐
│ Hit Location    │──▶ Determine body part
└─────────────────┘
    │
    ▼
┌─────────────────┐
│ Critical Check  │──▶ Bonus damage?
└─────────────────┘
    │
    ▼
┌─────────────────┐
│ Apply Damage    │──▶ Armor → Internal → Destroyed
└─────────────────┘
```

---

## Threading Model

Currently single-threaded with planned async support:

### Current (Single-threaded)

```
Main Thread
    │
    ├── Input Processing
    ├── Game Logic Update
    ├── Physics/Combat
    ├── AI Updates
    └── Rendering
```

### Planned (Multi-threaded)

```
Main Thread                 Worker Threads
    │                           │
    ├── Input Processing        │
    ├── Game Logic          ◀───┼─── Asset Loading (async)
    ├── Render Commands     ◀───┼─── Pathfinding (async)
    └── Present                 └─── Audio Mixing (async)
```

---

## Dependencies Between Modules

```
assets ◀── core ◀── game ◀── graphics
                       ▲        ▲
                       │        │
                    audio ◀─────┤
                              video
```

- **assets** - No dependencies (foundation)
- **core** - Depends on assets
- **game** - Depends on core, assets
- **graphics** - Depends on game, core, assets
- **audio** - Depends on core, assets
- **video** - Depends on core, assets
