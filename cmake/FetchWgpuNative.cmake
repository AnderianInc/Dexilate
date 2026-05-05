# ─────────────────────────────────────────────────────────────────────────────
#  cmake/FetchWgpuNative.cmake
#
#  Downloads wgpu-native prebuilt binaries from the official GitHub release
#  and creates the imported CMake target `wgpu::native`.
#
#  Supported platforms:
#    macOS   arm64          → wgpu-macos-aarch64-release.zip  → libwgpu_native.a
#    macOS   x86_64         → wgpu-macos-x86_64-release.zip   → libwgpu_native.a
#    macOS   universal      → both archives lipo'd together
#    Windows x64 (MSVC)     → wgpu-windows-x86_64-msvc-release.zip → wgpu_native.lib
#    Linux   x86_64         → wgpu-linux-x86_64-release.zip   → libwgpu_native.a
#    Linux   aarch64        → wgpu-linux-aarch64-release.zip  → libwgpu_native.a
#
#  Usage (consumers):
#    target_link_libraries(my_target PRIVATE wgpu::native)
# ─────────────────────────────────────────────────────────────────────────────

set(WGPU_VERSION "29.0.0.0")
set(WGPU_BASE_URL
    "https://github.com/gfx-rs/wgpu-native/releases/download/v${WGPU_VERSION}")

# ── macOS universal binary ────────────────────────────────────────────────────
if(APPLE AND CMAKE_OSX_ARCHITECTURES MATCHES "arm64" AND
            CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")

    message(STATUS "[Dexilate] wgpu-native: downloading both slices for universal binary")

    FetchContent_Declare(wgpu_native_arm64
        URL "${WGPU_BASE_URL}/wgpu-macos-aarch64-release.zip"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_Declare(wgpu_native_x86_64
        URL "${WGPU_BASE_URL}/wgpu-macos-x86_64-release.zip"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(wgpu_native_arm64 wgpu_native_x86_64)

    set(_WGPU_UNIVERSAL_DIR "${CMAKE_BINARY_DIR}/wgpu_native_universal")
    set(_WGPU_LIB           "${_WGPU_UNIVERSAL_DIR}/libwgpu_native.a")
    file(MAKE_DIRECTORY "${_WGPU_UNIVERSAL_DIR}")

    # lipo runs at configure time — both FetchContent archives are already local.
    execute_process(
        COMMAND lipo -create
            "${wgpu_native_arm64_SOURCE_DIR}/lib/libwgpu_native.a"
            "${wgpu_native_x86_64_SOURCE_DIR}/lib/libwgpu_native.a"
            -output "${_WGPU_LIB}"
        RESULT_VARIABLE _lipo_result
        ERROR_VARIABLE  _lipo_error
    )
    if(NOT _lipo_result EQUAL 0)
        message(FATAL_ERROR
            "[Dexilate] lipo failed creating wgpu-native universal binary:\n${_lipo_error}")
    endif()

    set(_WGPU_INCLUDE_DIR "${wgpu_native_arm64_SOURCE_DIR}/include")
    message(STATUS "[Dexilate] wgpu-native universal binary: ${_WGPU_LIB}")

# ── macOS single architecture ─────────────────────────────────────────────────
elseif(APPLE)

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64" OR
       CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        set(_WGPU_ARCHIVE "wgpu-macos-aarch64-release.zip")
    else()
        set(_WGPU_ARCHIVE "wgpu-macos-x86_64-release.zip")
    endif()

    FetchContent_Declare(wgpu_native
        URL "${WGPU_BASE_URL}/${_WGPU_ARCHIVE}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(wgpu_native)

    set(_WGPU_LIB         "${wgpu_native_SOURCE_DIR}/lib/libwgpu_native.a")
    set(_WGPU_INCLUDE_DIR "${wgpu_native_SOURCE_DIR}/include")

# ── Windows x64 ───────────────────────────────────────────────────────────────
elseif(WIN32)

    FetchContent_Declare(wgpu_native
        URL "${WGPU_BASE_URL}/wgpu-windows-x86_64-msvc-release.zip"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(wgpu_native)

    set(_WGPU_LIB         "${wgpu_native_SOURCE_DIR}/lib/wgpu_native.lib")
    set(_WGPU_INCLUDE_DIR "${wgpu_native_SOURCE_DIR}/include")

# ── Linux ─────────────────────────────────────────────────────────────────────
elseif(UNIX AND NOT APPLE)

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(_WGPU_ARCHIVE "wgpu-linux-aarch64-release.zip")
    else()
        set(_WGPU_ARCHIVE "wgpu-linux-x86_64-release.zip")
    endif()

    FetchContent_Declare(wgpu_native
        URL "${WGPU_BASE_URL}/${_WGPU_ARCHIVE}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(wgpu_native)

    set(_WGPU_LIB         "${wgpu_native_SOURCE_DIR}/lib/libwgpu_native.a")
    set(_WGPU_INCLUDE_DIR "${wgpu_native_SOURCE_DIR}/include")

else()
    message(FATAL_ERROR "[Dexilate] wgpu-native: unsupported platform")
endif()

# ── Validate the downloaded library exists ────────────────────────────────────
if(NOT EXISTS "${_WGPU_LIB}")
    message(FATAL_ERROR
        "[Dexilate] wgpu-native library not found after download: ${_WGPU_LIB}\n"
        "Check that the release archive for v${WGPU_VERSION} contains the expected file.")
endif()

# ── Imported target ───────────────────────────────────────────────────────────
add_library(wgpu::native STATIC IMPORTED GLOBAL)
set_target_properties(wgpu::native PROPERTIES
    IMPORTED_LOCATION             "${_WGPU_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_WGPU_INCLUDE_DIR}"
)

# ── Platform system library dependencies ─────────────────────────────────────
if(APPLE)
    # wgpu-native calls Metal, CoreFoundation, IOKit, etc. directly.
    set_property(TARGET wgpu::native APPEND PROPERTY INTERFACE_LINK_LIBRARIES
        "-framework Metal"
        "-framework QuartzCore"
        "-framework CoreFoundation"
        "-framework SystemConfiguration"
        "-framework IOKit"
        "-framework AppKit"
    )
elseif(WIN32)
    set_property(TARGET wgpu::native APPEND PROPERTY INTERFACE_LINK_LIBRARIES
        d3d12 dxgi d3dcompiler user32 ws2_32 advapi32 userenv ntdll opengl32
    )
elseif(UNIX AND NOT APPLE)
    find_package(Vulkan REQUIRED)
    set_property(TARGET wgpu::native APPEND PROPERTY INTERFACE_LINK_LIBRARIES
        Vulkan::Vulkan pthread dl m
    )
endif()

message(STATUS "[Dexilate] wgpu-native ${WGPU_VERSION} ready → ${_WGPU_LIB}")
