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

add_library(dexilate_core STATIC)
add_library(dexilate::core ALIAS dexilate_core)

target_include_directories(dexilate_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(dexilate_core PUBLIC cxx_std_20)

target_link_libraries(dexilate_core PUBLIC glm::glm)

# Sources added incrementally per phase — stub is intentionally empty.
