# ─────────────────────────────────────────────────────────────────────────────
#  src/engine/CMakeLists.txt
#  dexilate_engine — Layer 4 Engine
#
#  Raster compositor, vector renderer, document model, GPU pipeline.
#  Depends on dexilate_core and dexilate_platform; knows nothing about Qt.
#  Phase 1+: tile manager, brush engine, view transform
#  Phase 2+: layer compositor, history, selection
#  Phase 3+: scene graph, vector engine, text
#  Phase 4+: unified document, smart objects
#  Phase 5+: GPU filter pipeline
# ─────────────────────────────────────────────────────────────────────────────

add_library(dexilate_engine INTERFACE)
add_library(dexilate::engine ALIAS dexilate_engine)

target_include_directories(dexilate_engine
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(dexilate_engine INTERFACE cxx_std_20)

target_link_libraries(dexilate_engine
    INTERFACE
        dexilate::core
        dexilate::platform
        glm::glm
)

# Phase 0 stub — no sources yet. Promote to STATIC when Phase 1 adds files.
