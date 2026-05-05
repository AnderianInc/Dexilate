# Contributing to Dexilate

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| CMake | 3.28+ | `brew install cmake` / `winget install Kitware.CMake` |
| Ninja | latest | `brew install ninja` / `choco install ninja` |
| C++ compiler | Clang 16+ / GCC 13+ / MSVC 2022 | Must support C++20 |
| Qt6 | 6.7+ | See below — **not** fetched automatically |

## Installing Qt6

Qt6 must be installed separately and its location communicated to CMake via `Qt6_DIR`.

**macOS (Homebrew):**
```sh
brew install qt
export Qt6_DIR=$(brew --prefix qt)/lib/cmake/Qt6
```

**macOS (Qt Installer):**
```sh
export Qt6_DIR=~/Qt/6.7.2/macos/lib/cmake/Qt6
```

**Windows (Qt Installer):**
```bat
set Qt6_DIR=C:\Qt\6.7.2\msvc2022_64\lib\cmake\Qt6
```

**Linux (apt):**
```sh
sudo apt install qt6-base-dev qt6-opengl-dev
# Qt6_DIR is usually auto-detected; if not:
export Qt6_DIR=/usr/lib/x86_64-linux-gnu/cmake/Qt6
```

## Building

```sh
# Configure
cmake --preset macos-arm64        # or windows-x64, linux-x64, etc.

# Build
cmake --build build/macos-arm64 --parallel

# Run tests
ctest --preset macos-arm64
```

All third-party dependencies (Catch2, GLM, Clipper2, nlohmann/json) are fetched automatically via CMake FetchContent on first configure. Subsequent configures use the local cache.

## Project Structure

```
Dexilate/
├── cmake/                  # CMake modules (Version.h.in, Packaging.cmake)
├── src/
│   ├── platform/           # L2: OS abstraction (IWindow, IFileSystem, IClipboard)
│   │   ├── include/        # Public PAL headers
│   │   └── src/            # Platform-specific implementations
│   │       ├── macos/      # Cocoa/Metal (.mm files)
│   │       ├── windows/    # Win32/DXGI (.cpp files)
│   │       └── linux/      # X11/Wayland (.cpp files)
│   ├── core/               # L3: Codecs, color management, text (Phase 1+)
│   ├── engine/             # L4: Raster/vector engine, document model (Phase 1+)
│   ├── ui/                 # L5: Qt6 shell, panels, canvas widget (Phase 1+)
│   └── app/                # Entry point, DexilateApp lifecycle
├── tests/                  # Catch2 unit and integration tests
├── assets/                 # Shaders, icons, packaging assets (Phase 0+)
└── .github/workflows/      # CI (ci.yml) and CodeQL (codeql.yml)
```

## Code Standards

- C++20, no extensions (`-std=c++20` not `-std=gnu++20`)
- All source files must include an SPDX header: `// SPDX-License-Identifier: MIT`
- Minimum 80% line coverage for non-UI modules (enforced by CI)
- No OS API calls outside `src/platform/` — all OS interaction goes through the PAL interfaces

## Adding a New Feature

1. Check the phase it belongs to in `Dexilate_Project_Tracker.md`
2. Write the interface / header first, get it reviewed
3. Write tests before or alongside the implementation
4. Ensure CI is green on all three platforms before merging
