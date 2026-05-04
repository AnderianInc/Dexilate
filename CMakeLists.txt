cmake_minimum_required(VERSION 3.28)

# ─────────────────────────────────────────────────────────────────────────────
#  Canvas — Professional Creative Suite
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

project(Canvas
    VERSION      1.0.0
    DESCRIPTION  "Professional cross-platform creative suite (raster + vector)"
    HOMEPAGE_URL "https://github.com/your-org/canvas"
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
    "${CMAKE_CURRENT_BINARY_DIR}/generated/canvas/Version.h"
    @ONLY
)

# ─────────────────────────────────────────────────────────────────────────────
#  Platform detection & OS-specific settings
# ─────────────────────────────────────────────────────────────────────────────
if(APPLE)
    set(CANVAS_PLATFORM "macos")
    set(CANVAS_GPU_BACKEND "metal")

    # Minimum macOS deployment target
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "macOS minimum version")

    # Universal binary when building on Apple Silicon
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
    endif()

    message(STATUS "[Canvas] Platform: macOS | GPU: Metal | Arch: ${CMAKE_OSX_ARCHITECTURES}")

elseif(WIN32)
    set(CANVAS_PLATFORM "windows")
    set(CANVAS_GPU_BACKEND "d3d12")

    # Unicode Windows API
    add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    # Target Windows 10 (0x0A00) and above
    add_compile_definitions(_WIN32_WINNT=0x0A00)

    message(STATUS "[Canvas] Platform: Windows | GPU: Direct3D 12")

elseif(UNIX AND NOT APPLE)
    set(CANVAS_PLATFORM "linux")
    set(CANVAS_GPU_BACKEND "vulkan")

    # Required for Wayland + X11 support in Qt6
    find_package(PkgConfig REQUIRED)

    message(STATUS "[Canvas] Platform: Linux | GPU: Vulkan")

else()
    message(FATAL_ERROR "[Canvas] Unsupported platform. Target macOS, Windows, or Linux.")
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
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g -DNDEBUG>
    )
    # Enable AddressSanitizer in Debug builds (opt-in via cache var)
    option(CANVAS_ASAN "Enable AddressSanitizer (Debug only)" OFF)
    if(CANVAS_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
        message(STATUS "[Canvas] ASan + UBSan enabled")
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(
        /W4 /WX-          # High warnings, not errors (relax during early dev)
        /permissive-      # Strict standard conformance
        /Zc:__cplusplus   # Correct __cplusplus value
        /utf-8            # Source and execution charset
        $<$<CONFIG:Release>:/O2 /GL>
        $<$<CONFIG:RelWithDebInfo>:/O2 /Zi>
        $<$<CONFIG:Debug>:/Od /Zi /RTC1>
    )
    add_link_options($<$<CONFIG:Release>:/LTCG>)
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
find_package(Qt6 6.7 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    OpenGL       # For GPU surface hosting
    OpenGLWidgets
)
qt_standard_project_setup()
message(STATUS "[Canvas] Qt6 found: ${Qt6_VERSION} at ${Qt6_DIR}")

# ── Catch2 (test framework) ───────────────────────────────────────────────────
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# ── Clipper2 (vector boolean ops) ─────────────────────────────────────────────
FetchContent_Declare(Clipper2
    GIT_REPOSITORY https://github.com/AngusJohnson/Clipper2.git
    GIT_TAG        Clipper2_1.3.0
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  CPP
)
FetchContent_MakeAvailable(Clipper2)

# ── GLM (math: vectors, matrices, quaternions) ────────────────────────────────
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# ── nlohmann/json (settings, plugin manifests) ───────────────────────────────
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ── Note: These heavier deps will be added in later phases ────────────────────
# Skia        → Phase 1 (raster rendering)
# wgpu-native → Phase 1 (GPU pipeline)
# HarfBuzz    → Phase 3 (text shaping)
# FreeType    → Phase 3 (font rendering)
# LittleCMS2  → Phase 4 (color management)
# libjpeg-turbo, libpng, libtiff, libwebp, libavif, openexr → Phase 4 (codecs)

# ─────────────────────────────────────────────────────────────────────────────
#  Canvas library targets (subdirectories)
# ─────────────────────────────────────────────────────────────────────────────
#
#  Dependency graph (arrows = "depends on"):
#
#   canvas_app
#       ├── canvas_ui          (Qt6 shell: toolbars, panels, canvas widget)
#       │       └── canvas_engine
#       ├── canvas_engine      (raster + vector engine, document model)
#       │       ├── canvas_core
#       │       └── canvas_platform
#       ├── canvas_core        (codecs, color, text, math — no OS deps)
#       └── canvas_platform    (OS abstraction: window, input, filesystem)
#
add_subdirectory(src/platform)   # canvas_platform  — L2 PAL
add_subdirectory(src/core)       # canvas_core       — L3 core libs
add_subdirectory(src/engine)     # canvas_engine     — L4 raster/vector engine
add_subdirectory(src/ui)         # canvas_ui         — L5 Qt6 UI

# ── Main application executable ───────────────────────────────────────────────
add_subdirectory(src/app)        # canvas_app        — entry point

# ─────────────────────────────────────────────────────────────────────────────
#  Tests
# ─────────────────────────────────────────────────────────────────────────────
option(CANVAS_BUILD_TESTS "Build unit and integration tests" ON)
if(CANVAS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "[Canvas] Tests enabled")
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
message(STATUS "  Canvas ${PROJECT_VERSION}")
message(STATUS "  Platform  : ${CANVAS_PLATFORM}")
message(STATUS "  GPU       : ${CANVAS_GPU_BACKEND}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler  : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Tests     : ${CANVAS_BUILD_TESTS}")
message(STATUS "  Install   : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
message(STATUS "")
