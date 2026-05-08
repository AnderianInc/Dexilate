# ─────────────────────────────────────────────────────────────────────────────
#  src/engine/CMakeLists.txt
#  dexilate_engine — Layer 4 Engine
#
#  Raster engine, document model, view transform.
#  Depends on dexilate_core and dexilate_platform; knows nothing about Qt or wgpu.
#  Phase 2+: layer compositor, history, selection
#  Phase 3+: scene graph, vector engine, text
#  Phase 4+: unified document, smart objects
# ─────────────────────────────────────────────────────────────────────────────

add_library(dexilate_engine STATIC
    src/raster/TileManager.cpp
    src/document/Layer.cpp
    src/document/Document.cpp
    src/raster/BrushEngine.cpp
    src/raster/Eraser.cpp
    src/raster/InputSampler.cpp
    src/viewport/ViewTransform.cpp
)
add_library(dexilate::engine ALIAS dexilate_engine)

target_include_directories(dexilate_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(dexilate_engine PUBLIC cxx_std_20)

target_link_libraries(dexilate_engine
    PUBLIC
        dexilate::core
        dexilate::platform
        glm::glm
)
