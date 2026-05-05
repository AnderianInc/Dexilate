# ─────────────────────────────────────────────────────────────────────────────
#  src/platform/CMakeLists.txt
#  dexilate_platform — Layer 2 Platform Abstraction Library
#
#  Provides OS-native implementations of IWindow, IFileSystem, IClipboard,
#  and IInputSystem. All other dexilate libraries depend on this target for
#  any interaction with the operating system.
# ─────────────────────────────────────────────────────────────────────────────

add_library(dexilate_platform STATIC)
add_library(dexilate::platform ALIAS dexilate_platform)

# ── Public headers (available to all dependents) ──────────────────────────────
target_include_directories(dexilate_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/generated   # for dexilate/Version.h
)

# ── Common platform-independent sources ───────────────────────────────────────
target_sources(dexilate_platform PRIVATE
    src/Platform.cpp
)

# ── Platform-specific sources ─────────────────────────────────────────────────
if(APPLE)
    target_sources(dexilate_platform PRIVATE
        src/macos/MacWindow.mm
        src/macos/MacFileSystem.mm
        src/macos/MacClipboard.mm
    )
    target_link_libraries(dexilate_platform PUBLIC
        "-framework Cocoa"
        "-framework Metal"
        "-framework QuartzCore"
        "-framework AppKit"
        "-framework CoreGraphics"
        "-framework UniformTypeIdentifiers"
    )

elseif(WIN32)
    target_sources(dexilate_platform PRIVATE
        src/windows/Win32Window.cpp
        src/windows/Win32FileSystem.cpp
        src/windows/Win32Clipboard.cpp
    )
    target_link_libraries(dexilate_platform PUBLIC
        user32 shell32 shlwapi ole32 oleaut32 d3d12 dxgi
    )

elseif(UNIX AND NOT APPLE)
    # X11Window.cpp always compiled; WaylandWindow.cpp only when Wayland libs
    # are present (guarded by #ifdef DEXILATE_WAYLAND inside the file).
    target_sources(dexilate_platform PRIVATE
        src/linux/X11Window.cpp
        src/linux/LinuxFileSystem.cpp
        src/linux/LinuxClipboard.cpp
    )
    find_package(X11 REQUIRED)
    target_link_libraries(dexilate_platform PUBLIC X11::X11)

    # Wayland is optional — X11/XWayland is the default on Ubuntu 22.04 LTS.
    # Enable native Wayland support when the libraries are present.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Wayland IMPORTED_TARGET
        wayland-client wayland-egl xkbcommon)
    if(Wayland_FOUND)
        target_sources(dexilate_platform PRIVATE src/linux/WaylandWindow.cpp)
        target_link_libraries(dexilate_platform PUBLIC PkgConfig::Wayland)
        target_compile_definitions(dexilate_platform PUBLIC DEXILATE_WAYLAND=1)
        message(STATUS "[Dexilate] Wayland support enabled")
    else()
        message(STATUS "[Dexilate] Wayland not found — using X11 only")
    endif()
endif()

# ── C++ standard ──────────────────────────────────────────────────────────────
target_compile_features(dexilate_platform PUBLIC cxx_std_20)

# ── GLM (math types used in input coordinates) ────────────────────────────────
target_link_libraries(dexilate_platform PUBLIC glm::glm)
