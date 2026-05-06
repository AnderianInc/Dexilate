# ─────────────────────────────────────────────────────────────────────────────
#  src/core/CMakeLists.txt
#  dexilate_core — Layer 3 Core Library
#
#  Stateless C++ modules: color management, image codecs, text shaping,
#  and math utilities. No OS API calls — depends only on third-party libs.
#  Phase 1+: codecs (libpng, libjpeg-turbo, libtiff, libwebp, libavif, openexr)
#  Phase 3+: text (HarfBuzz, FreeType)
#  Phase 4+: color management (LittleCMS 2)
# ─────────────────────────────────────────────────────────────────────────────

add_library(dexilate_core INTERFACE)
add_library(dexilate::core ALIAS dexilate_core)

target_include_directories(dexilate_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(dexilate_core INTERFACE cxx_std_20)

target_link_libraries(dexilate_core INTERFACE glm::glm)

# Phase 0 stub — no sources yet. Promote to STATIC when Phase 1 adds files.
