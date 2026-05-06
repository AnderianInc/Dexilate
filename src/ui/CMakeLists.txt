# ─────────────────────────────────────────────────────────────────────────────
#  src/ui/CMakeLists.txt
#  dexilate_ui — Layer 5 Qt6 Shell
#
#  MainWindow, DexilateWidget (GPU surface host), toolbars, panels, dialogs.
#  Thin layer — contains no business logic; all state from dexilate_engine.
#  Phase 1+: MainWindow, DexilateWidget, ToolOptionsBar, StatusBar
#  Phase 2+: LayerPanel, HistoryPanel
#  Phase 3+: FontPicker, StrokeFillPanel
#  Phase 4+: ColorProfilePanel
# ─────────────────────────────────────────────────────────────────────────────

add_library(dexilate_ui INTERFACE)
add_library(dexilate::ui ALIAS dexilate_ui)

target_include_directories(dexilate_ui
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(dexilate_ui INTERFACE cxx_std_20)

target_link_libraries(dexilate_ui
    INTERFACE
        dexilate::engine
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
)

# Phase 0 stub — no sources yet. Promote to STATIC when Phase 1 adds files.
