cmake_minimum_required(VERSION 3.28)

# ─────────────────────────────────────────────────────────────────────────────
#  Dexilate — Professional Creative Suite
#  Root CMakeLists.txt
#
#  Supported platforms:
#    macOS  13+  (Metal GPU, arm64 + x86_64 universal binary)
#    Windows 11  (Direct3D 12, x64)
#    Linux       (Vulkan, x64 + arm64)
#
#  Build prerequisites:
#    CMake 3.28+, Ninja, C++20 compiler
#    vcpkg (for third-party deps) OR FetchContent (auto-download fallback)
#
#  Quick start:
#    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release --preset <platform>
#    cmake --build build --parallel
#
#  Available presets (see CMakePresets.json):
#    macos-arm64, macos-x64, windows-x64, linux-x64
# ─────────────────────────────────────────────────────────────────────────────

project(Dexilate
    VERSION      1.0.0
    DESCRIPTION  "Professional cross-platform creative suite (raster + vector)"
    HOMEPAGE_URL "https://github.com/your-org/dexilate"
    LANGUAGES    CXX C
)

# ── C++20 globally ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)   # -std=c++20, not -std=gnu++20

# ── Build type default ────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Build type not set — defaulting to RelWithDebInfo")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose: Debug Release RelWithDebInfo MinSizeRel" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

# ── Output directories ────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/lib)

# ── Module path ───────────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ── Version header ────────────────────────────────────────────────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/dexilate/Version.h"
    @ONLY
)

# ─────────────────────────────────────────────────────────────────────────────
#  Platform detection & OS-specific settings
# ─────────────────────────────────────────────────────────────────────────────
if(APPLE)
    set(DEXILATE_PLATFORM "macos")
    set(DEXILATE_GPU_BACKEND "metal")

    # Minimum macOS deployment target
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "macOS minimum version")

    # Universal binary when building on Apple Silicon
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
    endif()

    message(STATUS "[Dexilate] Platform: macOS | GPU: Metal | Arch: ${CMAKE_OSX_ARCHITECTURES}")

elseif(WIN32)
    set(DEXILATE_PLATFORM "windows")
    set(DEXILATE_GPU_BACKEND "d3d12")

    # Unicode Windows API
    add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    # Target Windows 10 (0x0A00) and above
    add_compile_definitions(_WIN32_WINNT=0x0A00)

    message(STATUS "[Dexilate] Platform: Windows | GPU: Direct3D 12")

elseif(UNIX AND NOT APPLE)
    set(DEXILATE_PLATFORM "linux")
    set(DEXILATE_GPU_BACKEND "vulkan")

    # Required for Wayland + X11 support in Qt6
    find_package(PkgConfig REQUIRED)
    # Required for wgpu-native Vulkan backend
    find_package(Vulkan REQUIRED)

    message(STATUS "[Dexilate] Platform: Linux | GPU: Vulkan | Vulkan SDK: ${Vulkan_VERSION}")

else()
    message(FATAL_ERROR "[Dexilate] Unsupported platform. Target macOS, Windows, or Linux.")
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  Compiler flags
# ─────────────────────────────────────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter          # Too noisy during early development
        -Wno-missing-field-initializers
        $<$<CONFIG:Debug>:-g3>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:RelWithDebInfo>:-O2>
        $<$<CONFIG:RelWithDebInfo>:-g>
    )
    # Enable AddressSanitizer in Debug builds (opt-in via cache var)
    option(DEXILATE_ASAN "Enable AddressSanitizer (Debug only)" OFF)
    if(DEXILATE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
        message(STATUS "[Dexilate] ASan + UBSan enabled")
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(
        /W4 /WX-          # High warnings, not errors (relax during early dev)
        /permissive-      # Strict standard conformance
        /Zc:__cplusplus   # Correct __cplusplus value
        /utf-8            # Source and execution charset
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:RelWithDebInfo>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/Zi>
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Debug>:/RTC1>
    )
    add_link_options($<$<CONFIG:Release>:/LTCG>)
    # MSVC ASan (available since VS 2019 16.9; no link flag needed — runtime is injected)
    option(DEXILATE_ASAN "Enable AddressSanitizer (Debug only)" OFF)
    if(DEXILATE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/fsanitize=address)
        message(STATUS "[Dexilate] MSVC ASan enabled")
    endif()
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  Third-party dependencies
# ─────────────────────────────────────────────────────────────────────────────
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)     # Show download progress

# ── Qt6 ──────────────────────────────────────────────────────────────────────
# Qt6 is NOT fetched via FetchContent — it must be installed separately.
# Set Qt6_DIR or CMAKE_PREFIX_PATH to your Qt6 installation.
# Example: cmake -DCMAKE_PREFIX_PATH=/opt/Qt/6.7.0/gcc_64 ...
find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    OpenGL       # For GPU surface hosting
    OpenGLWidgets
)
message(STATUS "[Dexilate] Qt6 found: ${Qt6_VERSION} at ${Qt6_DIR}")
# qt_standard_project_setup() intentionally NOT called here. Calling it in the
# root scope sets CMAKE_WIN32_EXECUTABLE=TRUE globally, which breaks Catch2 test
# entry points on Windows (tests expect main(), not WinMain()). It also enables
# CMAKE_AUTOMOC project-wide, adding unnecessary MOC overhead to non-Qt targets.
# Each Qt-using subdirectory calls qt_standard_project_setup() in its own scope.

# ── Catch2 (test framework) ───────────────────────────────────────────────────
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# ── Clipper2 (vector boolean ops) ─────────────────────────────────────────────
set(CLIPPER2_TESTS    OFF CACHE BOOL "" FORCE)
set(CLIPPER2_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(Clipper2
    GIT_REPOSITORY https://github.com/AngusJohnson/Clipper2.git
    GIT_TAG        Clipper2_1.3.0
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  CPP
)
FetchContent_MakeAvailable(Clipper2)

# Clipper2 1.3.0 adds -Werror to its own targets. Its Point<T> constructor uses
# template-id syntax that is ill-formed in C++20 under -Wpedantic. Demote that
# specific error back to a warning so only our code is held to -Werror.
foreach(_tgt IN ITEMS Clipper2 Clipper2Z)
    if(TARGET ${_tgt})
        target_compile_options(${_tgt} PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-error>)
    endif()
endforeach()

# ── GLM (math: vectors, matrices, quaternions) ────────────────────────────────
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# ── nlohmann/json (settings, plugin manifests) ───────────────────────────────
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)
# Provides: nlohmann_json::nlohmann_json (INTERFACE target)

# ── wgpu-native (GPU abstraction: Metal / D3D12 / Vulkan) ─────────────────────
# Downloads prebuilt binaries from the official GitHub release.
# Creates imported target: wgpu::native
# See cmake/FetchWgpuNative.cmake for platform details and universal binary handling.
include(cmake/FetchWgpuNative.cmake)

# ── Skia (CPU-side path rasterizer, export-quality rendering) ─────────────────
# Phase 0: DEXILATE_ENABLE_SKIA=OFF — a stub skia::skia target is created.
# Phase 1: set -DDEXILATE_ENABLE_SKIA=ON and choose an integration method.
# See cmake/FetchSkia.cmake for options (build-from-source, vcpkg, prebuilt).
include(cmake/FetchSkia.cmake)

# ── Deferred to later phases ──────────────────────────────────────────────────
# HarfBuzz 8.x  → Phase 3 (OpenType text shaping)
# FreeType 2.13 → Phase 3 (glyph rendering)
# LittleCMS 2.16 → Phase 4 (ICC color management)
# libjpeg-turbo, libpng 1.6, libtiff 4.6, libwebp, libavif, openexr 3.2 → Phase 4 (codecs)

# ─────────────────────────────────────────────────────────────────────────────
#  Dexilate library targets (subdirectories)
# ─────────────────────────────────────────────────────────────────────────────
#
#  Dependency graph (arrows = "depends on"):
#
#   dexilate_app
#       ├── dexilate_ui          (Qt6 shell: toolbars, panels, canvas widget)
#       │       └── dexilate_engine
#       ├── dexilate_engine      (raster + vector engine, document model)
#       │       ├── dexilate_core
#       │       └── dexilate_platform
#       ├── dexilate_core        (codecs, color, text, math — no OS deps)
#       └── dexilate_platform    (OS abstraction: window, input, filesystem)
#
add_subdirectory(src/platform)   # dexilate_platform  — L2 PAL
add_subdirectory(src/core)       # dexilate_core       — L3 core libs
add_subdirectory(src/engine)     # dexilate_engine     — L4 raster/vector engine
add_subdirectory(src/ui)         # dexilate_ui         — L5 Qt6 UI

# ── Main application executable ───────────────────────────────────────────────
add_subdirectory(src/app)        # dexilate_app        — entry point

# ─────────────────────────────────────────────────────────────────────────────
#  Tests
# ─────────────────────────────────────────────────────────────────────────────
option(DEXILATE_BUILD_TESTS "Build unit and integration tests" ON)
if(DEXILATE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "[Dexilate] Tests enabled")
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  Installation & Packaging (CPack)
# ─────────────────────────────────────────────────────────────────────────────
include(cmake/Packaging.cmake)

# ─────────────────────────────────────────────────────────────────────────────
#  Summary
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
message(STATUS "  Dexilate ${PROJECT_VERSION}")
message(STATUS "  Platform  : ${DEXILATE_PLATFORM}")
message(STATUS "  GPU       : ${DEXILATE_GPU_BACKEND}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler  : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Tests     : ${DEXILATE_BUILD_TESTS}")
message(STATUS "  Install   : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
message(STATUS "")
