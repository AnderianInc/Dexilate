# ─────────────────────────────────────────────────────────────────────────────
#  src/core/CMakeLists.txt
#  dexilate_core — Layer 3 Core Library
#
#  Stateless C++ modules: image codecs, tile data, math utilities.
#  No OS API calls. Depends only on third-party libs (libpng, GLM).
#  Phase 3+: text (HarfBuzz, FreeType)
#  Phase 4+: color management (LittleCMS 2), additional codecs
# ─────────────────────────────────────────────────────────────────────────────

add_library(dexilate_core STATIC
    src/Tile.cpp
    src/codecs/PngCodec.cpp
)
add_library(dexilate::core ALIAS dexilate_core)

target_include_directories(dexilate_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(dexilate_core PUBLIC cxx_std_20)

target_link_libraries(dexilate_core
    PUBLIC  glm::glm
    PRIVATE PNG::PNG
)

# pnglibconf.h lives in libpng's binary dir (generated at configure time),
# but libpng only exposes its source dir in INTERFACE_INCLUDE_DIRECTORIES.
# On Linux, /usr/include has a system copy as a silent fallback; macOS does
# not.  FetchContent_GetProperties reads global CMake properties so this
# works from any subdirectory regardless of where MakeAvailable was called.
include(FetchContent)
FetchContent_GetProperties(_dex_png)
if(_dex_png_POPULATED AND _dex_png_BINARY_DIR)
    target_include_directories(dexilate_core PRIVATE "${_dex_png_BINARY_DIR}")
endif()
