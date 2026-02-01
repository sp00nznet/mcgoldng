# FindSDL2_mixer.cmake
# Locate SDL2_mixer library
#
# This module defines:
#   SDL2_mixer_FOUND - System has SDL2_mixer
#   SDL2_mixer_INCLUDE_DIRS - SDL2_mixer include directories
#   SDL2_mixer_LIBRARIES - Libraries needed to use SDL2_mixer
#
# Also creates imported target SDL2_mixer::SDL2_mixer

find_path(SDL2_mixer_INCLUDE_DIR
    NAMES SDL_mixer.h
    PATH_SUFFIXES SDL2 include/SDL2 include
)

find_library(SDL2_mixer_LIBRARY
    NAMES SDL2_mixer
    PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2_mixer
    REQUIRED_VARS SDL2_mixer_LIBRARY SDL2_mixer_INCLUDE_DIR
)

if(SDL2_mixer_FOUND AND NOT TARGET SDL2_mixer::SDL2_mixer)
    add_library(SDL2_mixer::SDL2_mixer UNKNOWN IMPORTED)
    set_target_properties(SDL2_mixer::SDL2_mixer PROPERTIES
        IMPORTED_LOCATION "${SDL2_mixer_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_mixer_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(SDL2_mixer_INCLUDE_DIR SDL2_mixer_LIBRARY)

set(SDL2_mixer_INCLUDE_DIRS ${SDL2_mixer_INCLUDE_DIR})
set(SDL2_mixer_LIBRARIES ${SDL2_mixer_LIBRARY})
