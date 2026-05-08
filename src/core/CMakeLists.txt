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
