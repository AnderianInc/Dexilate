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

    # Expose cache vars so libpng's own find_package(ZLIB) resolves to our copy
    set(ZLIB_INCLUDE_DIR  "${_dex_zlib_SOURCE_DIR};${_dex_zlib_BINARY_DIR}"
        CACHE PATH "" FORCE)
    set(ZLIB_LIBRARY      zlibstatic CACHE FILEPATH "" FORCE)
    set(ZLIB_FOUND        TRUE       CACHE BOOL     "" FORCE)

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

if(TARGET png_static AND NOT TARGET PNG::PNG)
    add_library(PNG::PNG ALIAS png_static)
    message(STATUS "[Dexilate] libpng 1.6.43 ready (png_static)")
endif()
