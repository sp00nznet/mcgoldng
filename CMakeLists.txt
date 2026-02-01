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

# Optional packages for the main game
find_package(SDL2 CONFIG QUIET)
find_package(SDL2_mixer CONFIG QUIET)
find_package(OpenGL QUIET)
find_package(GLEW QUIET)

# Core asset library (used by both tools and game)
add_library(mcgng_assets STATIC
    src/assets/lz_decompress.cpp
    src/assets/fst_reader.cpp
    src/assets/pak_reader.cpp
    src/assets/fit_parser.cpp
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

    if(MSVC)
        target_compile_options(mcg-extract PRIVATE /W4)
    else()
        target_compile_options(mcg-extract PRIVATE -Wall -Wextra)
    endif()
endif()

# Main game executable (only if SDL2 and OpenGL are available)
if(SDL2_FOUND AND OPENGL_FOUND AND GLEW_FOUND)
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

    add_library(mcgng_graphics STATIC
        src/graphics/renderer.cpp
        src/graphics/sprite.cpp
        src/graphics/terrain.cpp
        src/graphics/ui.cpp
    )

    target_include_directories(mcgng_graphics PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(mcgng_graphics PUBLIC
        mcgng_core
        SDL2::SDL2
        OpenGL::GL
        GLEW::GLEW
    )

    add_library(mcgng_audio STATIC
        src/audio/audio_system.cpp
        src/audio/music_manager.cpp
    )

    target_include_directories(mcgng_audio PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(mcgng_audio PUBLIC
        mcgng_core
        SDL2_mixer::SDL2_mixer
    )

    add_library(mcgng_video STATIC
        src/video/smk_player.cpp
    )

    target_include_directories(mcgng_video PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(mcgng_video PUBLIC
        mcgng_core
    )

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

    add_executable(mcgoldng
        src/main.cpp
    )

    target_link_libraries(mcgoldng PRIVATE
        mcgng_core
        mcgng_graphics
        mcgng_audio
        mcgng_video
        mcgng_game
        SDL2::SDL2main
    )
else()
    message(STATUS "SDL2, OpenGL, or GLEW not found - building tools only")
endif()

# Tests
if(MCGNG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
