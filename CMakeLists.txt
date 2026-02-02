cmake_minimum_required(VERSION 3.16)
project(mcgoldng VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(MCGNG_BUILD_TOOLS "Build extraction and conversion tools" ON)
option(MCGNG_BUILD_TESTS "Build unit tests" OFF)
option(MCGNG_USE_SYSTEM_ZLIB "Use system zlib if available" ON)

# Platform-specific settings
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DNOMINMAX)
endif()

# Find zlib (optional - we have a fallback LZ implementation)
if(MCGNG_USE_SYSTEM_ZLIB)
    find_package(ZLIB QUIET)
endif()

if(ZLIB_FOUND)
    message(STATUS "Found ZLIB: ${ZLIB_LIBRARIES}")
    set(MCGNG_HAS_ZLIB TRUE)
    add_definitions(-DMCGNG_HAS_ZLIB)
else()
    message(STATUS "ZLIB not found - using built-in LZ decompression only")
    set(MCGNG_HAS_ZLIB FALSE)
endif()

# Find SDL2 (required for graphics/audio)
find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
    # Try alternative find methods
    find_package(SDL2 QUIET)
endif()

if(SDL2_FOUND)
    message(STATUS "Found SDL2")
    set(MCGNG_HAS_SDL2 TRUE)
    add_definitions(-DMCGNG_HAS_SDL2)
else()
    message(STATUS "SDL2 not found - graphics disabled")
    set(MCGNG_HAS_SDL2 FALSE)
endif()

# Find SDL2_mixer (optional - for audio)
find_package(SDL2_mixer CONFIG QUIET)
if(SDL2_mixer_FOUND)
    message(STATUS "Found SDL2_mixer")
    set(MCGNG_HAS_SDL2_MIXER TRUE)
    add_definitions(-DMCGNG_HAS_SDL2_MIXER)
else()
    message(STATUS "SDL2_mixer not found - audio disabled")
    set(MCGNG_HAS_SDL2_MIXER FALSE)
endif()

# Core asset library (used by both tools and game)
add_library(mcgng_assets STATIC
    src/assets/lz_decompress.cpp
    src/assets/fst_reader.cpp
    src/assets/pak_reader.cpp
    src/assets/fit_parser.cpp
    src/assets/shape_reader.cpp
    src/assets/nested_pak_reader.cpp
)

target_include_directories(mcgng_assets PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MCGNG_HAS_ZLIB)
    target_link_libraries(mcgng_assets PUBLIC ZLIB::ZLIB)
endif()

if(MSVC)
    target_compile_options(mcgng_assets PRIVATE /W4)
else()
    target_compile_options(mcgng_assets PRIVATE -Wall -Wextra)
endif()

# Build tools
if(MCGNG_BUILD_TOOLS)
    add_executable(mcg-extract
        tools/mcg_extract.cpp
    )

    target_link_libraries(mcg-extract PRIVATE
        mcgng_assets
    )

    add_executable(pak-inspect
        tools/pak_inspect.cpp
    )

    add_executable(mech-analyze
        tools/mech_analyze.cpp
    )

    if(MSVC)
        target_compile_options(mcg-extract PRIVATE /W4)
        target_compile_options(pak-inspect PRIVATE /W4)
    else()
        target_compile_options(mcg-extract PRIVATE -Wall -Wextra)
        target_compile_options(pak-inspect PRIVATE -Wall -Wextra)
    endif()
endif()

# Core engine library (always built, stubs when SDL2 not available)
add_library(mcgng_core STATIC
    src/core/engine.cpp
    src/core/config.cpp
    src/core/memory.cpp
)

target_include_directories(mcgng_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mcgng_core PUBLIC
    mcgng_assets
)

if(MCGNG_HAS_SDL2)
    target_link_libraries(mcgng_core PUBLIC SDL2::SDL2)
endif()

if(MSVC)
    target_compile_options(mcgng_core PRIVATE /W4)
else()
    target_compile_options(mcgng_core PRIVATE -Wall -Wextra)
endif()

# Graphics library (works with or without SDL2)
add_library(mcgng_graphics STATIC
    src/graphics/renderer.cpp
    src/graphics/sprite.cpp
    src/graphics/palette.cpp
    src/graphics/terrain.cpp
    src/graphics/ui.cpp
)

target_include_directories(mcgng_graphics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mcgng_graphics PUBLIC
    mcgng_core
)

if(MCGNG_HAS_SDL2)
    target_link_libraries(mcgng_graphics PUBLIC SDL2::SDL2)
endif()

if(MSVC)
    target_compile_options(mcgng_graphics PRIVATE /W4)
else()
    target_compile_options(mcgng_graphics PRIVATE -Wall -Wextra)
endif()

# Audio library
add_library(mcgng_audio STATIC
    src/audio/audio_system.cpp
    src/audio/music_manager.cpp
)

target_include_directories(mcgng_audio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mcgng_audio PUBLIC
    mcgng_core
)

if(MCGNG_HAS_SDL2_MIXER)
    target_link_libraries(mcgng_audio PUBLIC SDL2_mixer::SDL2_mixer)
endif()

if(MSVC)
    target_compile_options(mcgng_audio PRIVATE /W4)
else()
    target_compile_options(mcgng_audio PRIVATE -Wall -Wextra)
endif()

# Video library
add_library(mcgng_video STATIC
    src/video/smk_player.cpp
)

target_include_directories(mcgng_video PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mcgng_video PUBLIC
    mcgng_core
)

if(MSVC)
    target_compile_options(mcgng_video PRIVATE /W4)
else()
    target_compile_options(mcgng_video PRIVATE -Wall -Wextra)
endif()

# Game logic library
add_library(mcgng_game STATIC
    src/game/mission.cpp
    src/game/mech.cpp
    src/game/combat.cpp
)

target_include_directories(mcgng_game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mcgng_game PUBLIC
    mcgng_core
)

if(MSVC)
    target_compile_options(mcgng_game PRIVATE /W4)
else()
    target_compile_options(mcgng_game PRIVATE -Wall -Wextra)
endif()

# Main game executable
add_executable(mcgoldng
    src/main.cpp
)

target_link_libraries(mcgoldng PRIVATE
    mcgng_core
    mcgng_graphics
    mcgng_audio
    mcgng_video
    mcgng_game
)

if(MCGNG_HAS_SDL2)
    # Link SDL2main for proper entry point on Windows
    if(TARGET SDL2::SDL2main)
        target_link_libraries(mcgoldng PRIVATE SDL2::SDL2main)
    endif()
endif()

if(MSVC)
    target_compile_options(mcgoldng PRIVATE /W4)
else()
    target_compile_options(mcgoldng PRIVATE -Wall -Wextra)
endif()

# Summary
message(STATUS "")
message(STATUS "MCG-NG Configuration Summary:")
message(STATUS "  ZLIB:        ${MCGNG_HAS_ZLIB}")
message(STATUS "  SDL2:        ${MCGNG_HAS_SDL2}")
message(STATUS "  SDL2_mixer:  ${MCGNG_HAS_SDL2_MIXER}")
message(STATUS "  Build tools: ${MCGNG_BUILD_TOOLS}")
message(STATUS "")

# Tests
if(MCGNG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
