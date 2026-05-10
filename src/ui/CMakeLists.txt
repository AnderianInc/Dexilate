# ─────────────────────────────────────────────────────────────────────────────
#  src/ui/CMakeLists.txt
#  dexilate_ui — Layer 5 Qt6 Shell
#
#  MainWindow, CanvasWidget (software-render canvas), ToolOptionsBar, StatusBar.
#  Thin layer — no business logic; all state from dexilate_engine.
#  Phase 2+: LayerPanel, HistoryPanel
#  Phase 3+: FontPicker, StrokeFillPanel
# ─────────────────────────────────────────────────────────────────────────────

# AUTOMOC must be ON so Qt's moc processes Q_OBJECT headers automatically.
set(CMAKE_AUTOMOC ON)

add_library(dexilate_ui STATIC
    src/CanvasWidget.cpp
    src/ToolOptionsBar.cpp
    src/StatusBar.cpp
    src/MainWindow.cpp
    src/LayerPanel.cpp
    src/HistoryPanel.cpp
)
add_library(dexilate::ui ALIAS dexilate_ui)

# Headers live in src/ui/src/ — private to this library only.
# No public install headers needed (app includes via dexilate::ui directly).
target_include_directories(dexilate_ui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(dexilate_ui PUBLIC cxx_std_20)

target_link_libraries(dexilate_ui
    PUBLIC
        dexilate::engine
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
