# ─────────────────────────────────────────────────────────────────────────────
#  src/app/CMakeLists.txt
#  dexilate_app — Application entry point (Phase 1: Qt6 shell)
#
#  Phase 0: wgpu-only clear-pass loop (replaced by Phase 1 Qt6 app).
#  Phase 1: QApplication + DexilateApp + MainWindow; canvas via QPainter.
#  Phase 2+: wgpu GPU canvas integrated through CanvasWindow.
# ─────────────────────────────────────────────────────────────────────────────

set(CMAKE_AUTOMOC ON)

set(_app_sources
    main.cpp
    DexilateApp.cpp
    GpuContext.cpp
    gpu/TileUploader.cpp
)

if(APPLE)
    add_executable(dexilate_app MACOSX_BUNDLE ${_app_sources})
elseif(WIN32)
    add_executable(dexilate_app WIN32 ${_app_sources})
else()
    add_executable(dexilate_app ${_app_sources})
endif()

set_target_properties(dexilate_app PROPERTIES OUTPUT_NAME "dexilate")

target_compile_features(dexilate_app PRIVATE cxx_std_20)

# Local headers: GpuContext.h, DexilateApp.h, gpu/TileUploader.h
target_include_directories(dexilate_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(dexilate_app
    PRIVATE
        dexilate::ui
        dexilate::engine
        dexilate::platform
        wgpu::native
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
