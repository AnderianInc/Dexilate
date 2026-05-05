# ─────────────────────────────────────────────────────────────────────────────
#  cmake/FetchSkia.cmake
#
#  Skia integration — Phase 1 (CPU-side path rasterizer, export-quality render).
#
#  WHY SKIA IS NOT FETCHED AUTOMATICALLY
#  ───────────────────────────────────────
#  Skia uses the GN build system (not CMake) and has no official prebuilt
#  binary releases. The options for integration are:
#
#  Option A — Build from source (recommended for CI reproducibility):
#    Requires: Python 3.8+, GN tool, Ninja, ~8GB disk, 30–60 min build time.
#    Uses ExternalProject_Add to invoke GN + Ninja inside CMake.
#    Implementation: set DEXILATE_SKIA_BUILD_FROM_SOURCE=ON.
#
#  Option B — vcpkg (recommended for developer machines):
#    1. Install vcpkg and set VCPKG_ROOT.
#    2. vcpkg install skia
#    3. Pass -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
#    find_package(Skia CONFIG REQUIRED) will then resolve.
#
#  Option C — Prebuilt third-party binary (fast but least reproducible):
#    Some community projects publish CMake-friendly Skia binaries:
#      https://github.com/JetBrains/skia-build (used by JetBrains Compose)
#    Set DEXILATE_SKIA_PREBUILT_DIR to the extracted directory.
#
#  PHASE 0 BEHAVIOUR
#  ──────────────────
#  DEXILATE_ENABLE_SKIA defaults to OFF. A stub INTERFACE target `skia::skia`
#  is created so that engine code can link it unconditionally. When Skia is
#  off, no Skia API calls compile — guard with #ifdef DEXILATE_SKIA_ENABLED.
#
#  Enable for Phase 1:
#    cmake --preset <platform> -DDEXILATE_ENABLE_SKIA=ON \
#          -DDEXILATE_SKIA_BUILD_FROM_SOURCE=ON
# ─────────────────────────────────────────────────────────────────────────────

option(DEXILATE_ENABLE_SKIA "Enable Skia raster backend (required for Phase 1+)" OFF)

if(NOT DEXILATE_ENABLE_SKIA)
    # ── Stub target ───────────────────────────────────────────────────────────
    # Engine code links skia::skia unconditionally; this stub makes Phase 0
    # compile without Skia present. Guards (#ifdef DEXILATE_SKIA_ENABLED) prevent
    # any actual Skia API calls from reaching the linker.
    add_library(skia_stub INTERFACE)
    add_library(skia::skia ALIAS skia_stub)
    message(STATUS "[Dexilate] Skia disabled (Phase 0) — stub target active")
    return()
endif()

# ── Option A: build from source ───────────────────────────────────────────────
option(DEXILATE_SKIA_BUILD_FROM_SOURCE "Build Skia from source via GN (slow)" OFF)

if(DEXILATE_SKIA_BUILD_FROM_SOURCE)
    find_program(GN_EXECUTABLE gn REQUIRED
        HINTS "$ENV{HOME}/.local/bin" "/usr/local/bin" "/opt/homebrew/bin")
    find_program(PYTHON_EXECUTABLE python3 REQUIRED)

    set(SKIA_TAG "chrome/m116")

    include(ExternalProject)
    ExternalProject_Add(skia_external
        GIT_REPOSITORY "https://skia.googlesource.com/skia.git"
        GIT_TAG        "${SKIA_TAG}"
        GIT_SHALLOW    TRUE
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND
            ${PYTHON_EXECUTABLE} <SOURCE_DIR>/tools/git-sync-deps &&
            ${GN_EXECUTABLE} gen <BINARY_DIR>/out/Release
                --args=is_official_build=true\ is_debug=false\ skia_use_system_libjpeg_turbo=false\ skia_use_system_libpng=false\ skia_use_system_libwebp=false\ skia_use_system_zlib=false\ skia_use_system_harfbuzz=false\ skia_use_system_icu=false\ skia_enable_tools=false\ skia_enable_pdf=false
        BUILD_COMMAND
            ninja -C <BINARY_DIR>/out/Release skia
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "<BINARY_DIR>/out/Release/libskia.a"
    )

    ExternalProject_Get_Property(skia_external SOURCE_DIR BINARY_DIR)

    add_library(skia::skia STATIC IMPORTED GLOBAL)
    add_dependencies(skia::skia skia_external)
    set_target_properties(skia::skia PROPERTIES
        IMPORTED_LOCATION             "${BINARY_DIR}/out/Release/libskia.a"
        INTERFACE_INCLUDE_DIRECTORIES "${SOURCE_DIR}"
    )
    target_compile_definitions(skia::skia INTERFACE DEXILATE_SKIA_ENABLED=1)
    message(STATUS "[Dexilate] Skia: build-from-source mode (${SKIA_TAG})")

# ── Option B: vcpkg (find_package after toolchain) ────────────────────────────
elseif(DEFINED ENV{VCPKG_ROOT} OR DEFINED VCPKG_ROOT)
    find_package(Skia CONFIG QUIET)
    if(Skia_FOUND)
        if(NOT TARGET skia::skia)
            add_library(skia::skia ALIAS skia)
        endif()
        target_compile_definitions(skia::skia INTERFACE DEXILATE_SKIA_ENABLED=1)
        message(STATUS "[Dexilate] Skia: found via vcpkg at ${Skia_DIR}")
    else()
        message(FATAL_ERROR
            "[Dexilate] DEXILATE_ENABLE_SKIA=ON but Skia not found via vcpkg.\n"
            "Run: vcpkg install skia\n"
            "Then pass -DCMAKE_TOOLCHAIN_FILE=\$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake")
    endif()

# ── Option C: prebuilt binary directory ───────────────────────────────────────
elseif(DEFINED DEXILATE_SKIA_PREBUILT_DIR)
    if(NOT EXISTS "${DEXILATE_SKIA_PREBUILT_DIR}")
        message(FATAL_ERROR
            "[Dexilate] DEXILATE_SKIA_PREBUILT_DIR does not exist: ${DEXILATE_SKIA_PREBUILT_DIR}")
    endif()

    # Expect: <dir>/libskia.a (or skia.lib on Windows) + <dir>/include/
    if(WIN32)
        set(_SKIA_LIB "${DEXILATE_SKIA_PREBUILT_DIR}/skia.lib")
    else()
        set(_SKIA_LIB "${DEXILATE_SKIA_PREBUILT_DIR}/libskia.a")
    endif()

    if(NOT EXISTS "${_SKIA_LIB}")
        message(FATAL_ERROR
            "[Dexilate] Expected Skia library not found: ${_SKIA_LIB}\n"
            "Check DEXILATE_SKIA_PREBUILT_DIR layout.")
    endif()

    add_library(skia::skia STATIC IMPORTED GLOBAL)
    set_target_properties(skia::skia PROPERTIES
        IMPORTED_LOCATION             "${_SKIA_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${DEXILATE_SKIA_PREBUILT_DIR}/include"
    )
    target_compile_definitions(skia::skia INTERFACE DEXILATE_SKIA_ENABLED=1)
    message(STATUS "[Dexilate] Skia: prebuilt binary at ${DEXILATE_SKIA_PREBUILT_DIR}")

else()
    message(FATAL_ERROR
        "[Dexilate] DEXILATE_ENABLE_SKIA=ON but no integration method selected.\n"
        "Choose one:\n"
        "  -DDEXILATE_SKIA_BUILD_FROM_SOURCE=ON\n"
        "  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake  (and vcpkg install skia)\n"
        "  -DDEXILATE_SKIA_PREBUILT_DIR=<path>")
endif()
