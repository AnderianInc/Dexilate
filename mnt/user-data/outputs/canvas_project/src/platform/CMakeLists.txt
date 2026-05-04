# ─────────────────────────────────────────────────────────────────────────────
#  src/platform/CMakeLists.txt
#  canvas_platform — Layer 2 Platform Abstraction Library
#
#  Provides OS-native implementations of IWindow, IFileSystem, IClipboard,
#  and IInputSystem. All other canvas libraries depend on this target for
#  any interaction with the operating system.
# ─────────────────────────────────────────────────────────────────────────────

add_library(canvas_platform STATIC)
add_library(canvas::platform ALIAS canvas_platform)

# ── Public headers (available to all dependents) ──────────────────────────────
target_include_directories(canvas_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/generated   # for canvas/Version.h
)

# ── Common platform-independent sources ───────────────────────────────────────
target_sources(canvas_platform PRIVATE
    src/Platform.cpp        # Platform::init() / shutdown() entry points
)

# ── Platform-specific sources ─────────────────────────────────────────────────
if(APPLE)
    target_sources(canvas_platform PRIVATE
        src/macos/MacWindow.mm        # NSWindow + CAMetalLayer
        src/macos/MacFileSystem.mm    # NSOpenPanel, NSSavePanel
        src/macos/MacClipboard.mm     # NSPasteboard
    )
    target_link_libraries(canvas_platform PUBLIC
        "-framework Cocoa"
        "-framework Metal"
        "-framework QuartzCore"
        "-framework AppKit"
        "-framework UniformTypeIdentifiers"
    )

elseif(WIN32)
    target_sources(canvas_platform PRIVATE
        src/windows/Win32Window.cpp   # HWND + DXGI surface
        src/windows/Win32FileSystem.cpp
        src/windows/Win32Clipboard.cpp
    )
    target_link_libraries(canvas_platform PUBLIC
        user32 shell32 shlwapi ole32 oleaut32 d3d12 dxgi
    )

elseif(UNIX AND NOT APPLE)
    target_sources(canvas_platform PRIVATE
        src/linux/X11Window.cpp       # X11 window (Wayland via XWayland)
        src/linux/WaylandWindow.cpp   # Native Wayland window
        src/linux/LinuxFileSystem.cpp
        src/linux/LinuxClipboard.cpp
    )
    find_package(X11 REQUIRED)
    pkg_check_modules(Wayland REQUIRED IMPORTED_TARGET
        wayland-client wayland-egl xkbcommon)
    target_link_libraries(canvas_platform PUBLIC
        X11::X11
        PkgConfig::Wayland
    )
endif()

# ── C++ standard ─────────────────────────────────────────────────────────────
target_compile_features(canvas_platform PUBLIC cxx_std_20)

# ── GLM (math types used in input coordinates) ────────────────────────────────
target_link_libraries(canvas_platform PUBLIC glm::glm)
