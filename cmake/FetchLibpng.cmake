# ─────────────────────────────────────────────────────────────────────────────
#  cmake/FetchLibpng.cmake
#  Downloads zlib and libpng 1.6 via FetchContent.
#
#  After inclusion:
#    PNG::PNG  — alias for png_static (libpng headers + static lib)
#    ZLIB::ZLIB — alias for the zlib static lib (system or fetched)
#
#  zlib: prefer the system installation (macOS / Linux ship it); fall back to
#  a fetched copy (needed on Windows where no system zlib is guaranteed).
# ─────────────────────────────────────────────────────────────────────────────

include(FetchContent)

# ── zlib ─────────────────────────────────────────────────────────────────────
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    message(STATUS "[Dexilate] System zlib not found — fetching madler/zlib v1.3.1")
    FetchContent_Declare(_dex_zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG        v1.3.1
        GIT_SHALLOW    TRUE
    )
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(_dex_zlib)

    # Get the actual output name (madler/zlib uses OUTPUT_NAME "z", not "zlibstatic")
    get_target_property(_dex_zlib_outname zlibstatic OUTPUT_NAME)
    if(NOT _dex_zlib_outname)
        set(_dex_zlib_outname "zlibstatic")
    endif()

    # ZLIB_LIBRARY must be an *absolute* path so that find_library() inside
    # libpng's cmake skips the filesystem search.  find_library() only skips
    # when the cached value is an absolute path; a bare target name is not.
    set(ZLIB_INCLUDE_DIR
        "${_dex_zlib_SOURCE_DIR};${_dex_zlib_BINARY_DIR}"
        CACHE PATH "" FORCE)
    set(ZLIB_LIBRARY
        "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${CMAKE_STATIC_LIBRARY_PREFIX}${_dex_zlib_outname}${CMAKE_STATIC_LIBRARY_SUFFIX}"
        CACHE FILEPATH "" FORCE)
    set(ZLIB_FOUND TRUE CACHE BOOL "" FORCE)

    if(NOT TARGET ZLIB::ZLIB)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
else()
    message(STATUS "[Dexilate] System zlib found: ${ZLIB_VERSION_STRING}")
endif()

# ── libpng 1.6 ────────────────────────────────────────────────────────────────
set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_STATIC ON  CACHE BOOL "" FORCE)
set(PNG_TESTS  OFF CACHE BOOL "" FORCE)
set(PNG_TOOLS  OFF CACHE BOOL "" FORCE)

FetchContent_Declare(_dex_png
    GIT_REPOSITORY https://github.com/glennrp/libpng.git
    GIT_TAG        v1.6.43
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(_dex_png)

# libpng only adds its source dir to INTERFACE_INCLUDE_DIRECTORIES.
# pnglibconf.h is generated in the binary dir — expose it so all consumers
# (e.g. dexilate_core) can include it without a system-level fallback.
target_include_directories(png_static INTERFACE
    $<BUILD_INTERFACE:${_dex_png_BINARY_DIR}>
)

if(TARGET png_static AND NOT TARGET PNG::PNG)
    add_library(PNG::PNG ALIAS png_static)
    message(STATUS "[Dexilate] libpng 1.6.43 ready (png_static)")
endif()
