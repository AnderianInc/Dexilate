**DEXILATE**

Professional Creative Suite

Photoshop + Illustrator Alternative · Cross-Platform · Native C++

| **Document Type**<br><br>Project Tracker & Architecture Spec | **Version**<br><br>v1.0 - Pre-Development | **Date**<br><br>May 2026 |
| ------------------------------------------------------------ | ----------------------------------------- | ------------------------ |

# **1\. Executive Summary**

Dexilate is a professional-grade creative suite combining raster image editing (Photoshop-equivalent) and vector graphics (Illustrator-equivalent) into a single unified native application. The application targets designers, photographers, illustrators, and creative professionals requiring high-performance tools without subscription lock-in.

The application is built natively in C++20 using cross-platform libraries, delivering a single codebase that compiles and distributes on macOS, Windows, and Linux. This document defines the complete technical architecture, development phases, milestones, and risk register before any code is written.

**Core Value Proposition**

A single native application replacing Adobe Photoshop + Illustrator - no subscriptions, no cloud dependency, professional-grade performance on all three major operating systems with a perpetual license model.

## **1.1 Project Goals**

- Unified raster + vector editing in a single document model
- Native performance: 60fps canvas, GPU-accelerated compositing and filters
- Full cross-platform support: macOS 13+, Windows 11, Ubuntu 22.04 LTS
- Professional file format support: PSD, SVG, PDF, PNG, TIFF, JPEG, WebP, AVIF
- Non-destructive editing: adjustment layers, smart objects, live effects
- Color science: 16-bit / 32-bit HDR workflows, ICC profile support, CMYK
- Plugin/extension API for third-party tools

## **1.2 Out of Scope (v1.0)**

- Cloud sync / collaborative editing
- Video or motion graphics timeline
- Mobile (iOS / Android) clients
- AI generative features (planned for v2.0)

# **2\. Full System Architecture**

The architecture is organized into five horizontal layers, each with a clearly defined responsibility. Higher layers depend on lower layers; lower layers have no knowledge of higher layers. This strict dependency direction enables each layer to be tested independently.

## **2.1 Layer Overview**

| **Layer**     | **Name**                       | **Responsibility**                                                                    |
| ------------- | ------------------------------ | ------------------------------------------------------------------------------------- |
| **L5 (Top)**  | **UI Layer**                   | Dexilate viewport, toolbars, panels, dialogs. Thin layer; contains no business logic.   |
| **L4**        | **Engine Layer**               | Raster compositor, vector renderer, document model. Core creative logic.              |
| **L3**        | **Core Library Layer**         | Color management, render pipeline, text shaping, image codecs. Stateless C++ modules. |
| **L2**        | **Platform Abstraction Layer** | File system, clipboard, native dialogs, input events, GPU context creation.           |
| **L1 (Base)** | **OS / GPU Layer**             | macOS Metal, Windows D3D12, Linux Vulkan. Thin wrappers via wgpu-native.              |

## **2.2 UI Layer (L5)**

The UI layer is split into two subsystems that share the same process but render through different mechanisms.

### **2.2.1 Dexilate Viewport**

The central canvas is a raw GPU surface exposed via the platform window handle. It does not use any widget toolkit's paint system. Every frame is rendered by the GPU pipeline (L1), composited by the engine layer (L4), and presented to the screen at 60fps even on large documents.

- Pan, zoom, and rotate via affine transform matrices applied to the view frustum
- Raw pointer/touch/stylus events from the platform abstraction layer
- Tablet pressure sensitivity via WinTab (Windows), Apple Pencil API (macOS), libinput (Linux)
- HiDPI-aware: all coordinates in logical pixels, scaled by device pixel ratio at render time

### **2.2.2 Shell UI (Toolbars, Panels, Dialogs)**

The surrounding application chrome is built with Qt6 - the most mature cross-platform C++ UI toolkit, with native look-and-feel on all three target OSes, excellent HiDPI support, and proven use in professional creative apps (Affinity Suite, Kdenlive).

- Qt6 Widgets for traditional panel/toolbar UI (faster than QML for dense, data-heavy panels)
- Custom QWidget subclass hosts the raw GPU canvas surface via QWindow::fromWinId
- All UI state is driven by the document model; the UI layer never directly mutates document data

## **2.3 Engine Layer (L4)**

### **2.3.1 Raster Engine**

The raster engine implements a tile-based architecture where the document canvas is subdivided into 256×256 pixel tiles. Only dirty tiles are recomposited on each frame, enabling interactive performance on very large canvases (10,000×10,000 px at 300 DPI).

- Tile manager: tracks dirty regions, manages tile cache in GPU VRAM and system RAM
- Layer compositor: evaluates the layer stack applying all 20+ ISO blend modes per the PDF specification
- Brush engine: pressure-sensitive dab rendering with spacing, jitter, opacity, flow, and hardness
- GPU compute shaders handle per-pixel filter operations: Gaussian blur, unsharp mask, curves, levels, hue/saturation
- Smart objects: raster layers referencing external source data, resampled on demand

### **2.3.2 Vector Engine**

The vector engine maintains a scene graph of geometric objects. All vector data is resolution-independent and rasterized to tiles on demand.

- Scene graph: tree of nodes (paths, groups, text frames, placed images) with affine transforms
- Path representation: cubic Bezier curves stored as compact contour lists (moveTo, lineTo, curveTo, close)
- Stroking and filling: path stroker with dashed patterns; miter, round, and bevel joins
- Boolean operations: union, difference, intersection, and XOR via Clipper2 library
- Text on path: glyph placement along a Bezier curve using arc-length parameterization
- Live effects: non-destructive effects stack (blur, drop shadow, stroke, inner glow) on nodes

### **2.3.3 Unified Document Model**

The document model is the most architecturally critical component. It is the single source of truth for all document state and the primary design challenge of the entire project.

- Document tree: heterogeneous layer tree holding raster, vector, adjustment, group, text, and smart object layers
- History manager: command-pattern undo/redo stack with memory-bounded tile diffing
- Transaction system: groups of mutations applied atomically; canvas does not re-render until commit
- Observer pattern: UI components subscribe to document change events and update lazily
- Serialization: custom binary format (.canvas) with import/export modules for PSD, SVG, and PDF

## **2.4 Core Library Layer (L3)**

### **2.4.1 Color Management**

LittleCMS 2 handles all ICC color space conversions (RGB↔CMYK, sRGB↔Display P3↔ProPhoto RGB). The engine operates in scene-linear floating-point color space internally and converts to display color space at present time using the display's ICC profile.

### **2.4.2 Render Pipeline**

Skia (Google's open-source 2D graphics library, used in Chrome, Android, and Flutter) provides the CPU-side path rasterizer and GPU backend. Skia is used for export-quality rendering; real-time rendering uses the custom GPU pipeline.

### **2.4.3 Text and Font Shaping**

HarfBuzz handles OpenType shaping (ligatures, kerning, mark positioning, bidirectional text). FreeType renders glyph outlines. The pipeline supports all major scripts including Latin, Arabic, Hebrew, CJK, Indic, and emoji.

### **2.4.4 Image Codecs**

A unified codec layer wraps format-specific libraries behind a common decode/encode interface. All codecs support streaming decode for large files.

- JPEG: libjpeg-turbo (SIMD-accelerated, 3-6× faster than reference libjpeg)
- PNG: libpng 1.6 with zlib compression
- TIFF: libtiff 4.6 - 16-bit, 32-bit float, CMYK, multi-page, BigTIFF
- WebP: libwebp (Google's lossy/lossless codec)
- AVIF: libavif (AV1 Image File Format, HDR support)
- OpenEXR 3.x: industry HDR format for VFX workflows
- PSD: custom parser based on reverse-engineering and community documentation

## **2.5 Platform Abstraction Layer (L2)**

The PAL isolates all OS-specific code behind pure virtual C++ interfaces. Engine and core library code never call OS APIs directly.

- IFileSystem: file open/save dialogs, recent files, temp directory, atomic file writes
- IClipboard: copy/paste of images, paths, and text between Dexilate and other apps
- IWindowSystem: window creation, GPU surface creation, HiDPI scale factor, multi-monitor
- IInputSystem: keyboard, mouse, stylus, touch, and gamepad input normalization
- IPlatformPlugin: dynamic library loading for the third-party plugin system

**Build System**

CMake 3.28+ with CPack for packaging. CI runs on GitHub Actions: macOS (arm64 + x86_64 universal binary), Windows (x64), Ubuntu (x64 + arm64). Release artifacts: macOS .dmg (notarized), Windows NSIS installer, Linux .AppImage and .deb.

## **2.6 GPU Layer (L1)**

Direct GPU API usage is confined to this layer. wgpu-native translates to Metal on macOS, Direct3D 12 on Windows, and Vulkan on Linux. GPU shaders are written once in WGSL (WebGPU Shading Language) and compiled to native shaders at runtime.

- Compute shaders: per-tile filter operations run as GPU compute dispatches
- Render shaders: canvas compositing as a render pipeline pass drawing tiles as quads with blend mode shaders
- Texture atlases: glyph bitmaps, brush tip textures, and pattern fills packed into GPU texture atlases
- Memory management: tile VRAM budget configurable (default 512MB); tiles evicted LRU when over budget

# **3\. Technology Stack**

| **Component**           | **Technology**      | **Rationale**                                                                                   |
| ----------------------- | ------------------- | ----------------------------------------------------------------------------------------------- |
| **Language**            | C++20               | Performance, memory control, universal platform support, creative app library ecosystem.        |
| **Build System**        | CMake 3.28 + Ninja  | Industry standard, all target platforms, CI/CD integration, generates Xcode/VS solutions.       |
| **UI Toolkit**          | Qt 6.7 (LGPL)       | Most mature cross-platform C++ UI toolkit. Native menus, file dialogs, HiDPI, accessibility.    |
| **2D Rasterizer**       | Skia m116+          | Battle-tested in Chrome and Android. Path rasterization, text, GPU backend. Apache 2 license.   |
| **GPU Abstraction**     | wgpu-native 0.20    | Single API → Metal / D3D12 / Vulkan. Shader code in WGSL compiled at runtime.                   |
| **Vector Boolean Ops**  | Clipper2 1.3        | Robust polygon clipping. Handles self-intersections and winding edge cases. Boost license.      |
| **Font Shaping**        | HarfBuzz 8.x        | Industry-standard OpenType shaper. Used in Chrome, Firefox, LibreOffice. All scripts.           |
| **Font Rendering**      | FreeType 2.13       | Reference glyph rendering. Sub-pixel hinting, color fonts (COLRv1).                             |
| **Color Management**    | LittleCMS 2.16      | ISO-compliant ICC. RGB, CMYK, LAB, XYZ. Used by GIMP, Inkscape, Scribus.                        |
| **JPEG**                | libjpeg-turbo 3.x   | SIMD-accelerated. 3-6× faster than reference libjpeg. Used by Chrome, Firefox, ffmpeg.          |
| **PNG**                 | libpng 1.6          | Reference implementation. Stable, widely tested.                                                |
| **TIFF**                | libtiff 4.6         | Full TIFF spec: 16-bit, 32-bit float, CMYK, multi-page, BigTIFF.                                |
| **WebP / AVIF**         | libwebp + libavif   | Modern web-native formats. HDR support in AVIF. Required for web export workflows.              |
| **HDR Images**          | OpenEXR 3.2         | Industry standard for VFX/compositing. 16-bit half and 32-bit float per channel.                |
| **Testing**             | Catch2 + CTest      | Header-only, fast compilation. BDD-style tests. Integrates with CMake CTest.                    |
| **Packaging (macOS)**   | CPack + notarytool  | Generates .dmg, submits to Apple notarization. Required for Gatekeeper.                         |
| **Packaging (Windows)** | NSIS + CPack        | Generates .exe installer. Supports code signing with EV certificate.                            |
| **Packaging (Linux)**   | AppImageTool + dpkg | AppImage for universal distribution. .deb for Ubuntu PPA. Flatpak planned v1.1.                 |
| **CI/CD**               | GitHub Actions      | Free for open-source, matrix builds across all three OSes, artifact upload, release automation. |

# **4\. Development Phases**

The project is divided into six sequential phases. Each phase has a defined entry criterion (previous phase complete) and exit criterion (all tasks done, milestone met).

| **Phase**   | **Description**                                                                     | **Duration** | **Status**      | **Owner**        |
| ----------- | ----------------------------------------------------------------------------------- | ------------ | --------------- | ---------------- |
| **Phase 0** | Toolchain, CI/CD, and 'Hello Dexilate' - a GPU-rendered window on all three platforms | 2 weeks      | **Not Started** | Engineering Lead |
| **Phase 1** | Raster core: single layer, brush tool, eraser, zoom/pan, PNG open/save              | 8 weeks      | **Not Started** | Raster Team      |
| **Phase 2** | Layer system: blend modes, opacity, groups, adjustment layers, history/undo         | 10 weeks     | **Not Started** | Raster Team      |
| **Phase 3** | Vector engine: pen tool, shape tools, boolean ops, text, SVG import/export          | 12 weeks     | **Not Started** | Vector Team      |
| **Phase 4** | Unified document: mixed raster+vector, PSD import, PDF export, smart objects        | 10 weeks     | **Not Started** | Full Team        |
| **Phase 5** | Polish: GPU filter pipeline, color management, plugin API, packaging, QA            | 8 weeks      | **Not Started** | Full Team        |

## **4.1 Phase 0 - Toolchain & Foundation (Weeks 1-2)**

- Initialize CMake project with presets for macOS, Windows, Linux
- Set up GitHub Actions CI: matrix build, test, and artifact upload
- Integrate Qt6, wgpu-native, Skia as CMake FetchContent or vcpkg dependencies
- Implement PAL skeleton: IWindowSystem, IInputSystem stubs
- Render a white canvas surface on all three platforms via wgpu GPU clear pass
- Write PAL unit tests using Catch2

## **4.2 Phase 1 - Raster Core (Weeks 3-10)**

- Tile manager: 256×256 tile grid, dirty tracking, CPU-side tile buffers
- Brush engine: round brush with pressure-sensitive opacity and size
- Eraser: composites onto layer alpha channel
- Pan: click-drag pans the view transform; Zoom: scroll wheel 0.1×-64× range
- Document model: single-raster-layer document, width/height, pixel depth (8/16-bit)
- PNG codec: open PNG file into raster layer, save raster layer to PNG
- Basic Qt6 shell: menu bar (File → Open, Save, Quit), tool options bar, status bar

## **4.3 Phase 2 - Layer System (Weeks 11-20)**

- Layer panel: add, delete, reorder, rename, toggle visibility, lock layers
- Layer compositor: GPU-accelerated tile compositing with blend mode shaders (all 20 ISO blend modes)
- Opacity: per-layer opacity slider composited into blend pass
- Group layers: clipping mask, pass-through blend mode
- Adjustment layers: Curves, Levels, Hue/Saturation, Color Balance, Brightness/Contrast
- History panel: unlimited undo/redo, memory-bounded by tile diffing
- Selection tools: Rectangular Marquee, Lasso, Quick Selection (flood-fill based)
- Copy/paste: clipboard integration via PAL IClipboard

## **4.4 Phase 3 - Vector Engine (Weeks 21-32)**

- Scene graph: node tree, affine transforms, z-order, grouping
- Pen tool: click to add anchor points, drag to create Bezier handles, close path
- Node editor: select, move, add, delete anchor points; convert corner/smooth
- Shape tools: Rectangle, Ellipse, Polygon, Star, Line
- Boolean operations: union, difference, intersect, XOR (Clipper2)
- Stroke and fill: solid color, linear gradient, radial gradient, pattern fill
- Text tool: area text frame, point text; font picker; size, leading, tracking, kerning
- Text on path: glyphs flow along selected Bezier curve
- SVG import: parse SVG 1.1 into scene graph nodes
- SVG export: serialize scene graph to clean SVG 1.1 output

## **4.5 Phase 4 - Unified Document (Weeks 33-42)**

- Mixed layer tree: raster and vector layers coexist in one document
- Rasterize vector layer: convert scene graph node to raster layer at document resolution
- Smart objects: embed a vector document inside a raster document, edit non-destructively
- PSD import: parse PSD layer structure, raster data, adjustment layers, text layers
- PSD export: serialize document to PSD format (Photoshop-compatible)
- PDF export: flatten document to PDF/X-1a using Cairo PDF backend
- Color management: assign and convert ICC profiles per layer and per document
- Soft-proof: on-screen preview rendered in target output color space

## **4.6 Phase 5 - Polish & Release (Weeks 43-50)**

- GPU filter pipeline: Gaussian Blur, Motion Blur, Radial Blur, Sharpen, Noise as compute shader passes
- Non-destructive filters: smart filter layer stores parameters, re-evaluates on demand
- Plugin API: C ABI plugin interface; sample plugin; published plugin SDK
- Performance profiling: frame time budget analysis, tile eviction tuning, shader optimization
- Accessibility: screen reader labels on all UI elements via Qt Accessibility API
- macOS packaging: .dmg with drag-to-Applications, Apple notarization, App Store preparation
- Windows packaging: NSIS installer, code signing with EV certificate, Windows App Certification
- Linux packaging: AppImage (universal), .deb for Ubuntu PPA, Flatpak manifest
- QA: cross-platform test matrix, regression suite, user acceptance testing

# **5\. Milestones**

| **Milestone**               | **Target Date** | **Deliverable**                                                    | **Status**      |
| --------------------------- | --------------- | ------------------------------------------------------------------ | --------------- |
| **M0 - Hello Dexilate**       | Week 2          | GPU-rendered window opens on macOS, Windows, Linux from CI build   | **Not Started** |
| **M1 - First Brush Stroke** | Week 6          | Open PNG, paint with pressure-sensitive brush, save PNG            | **Not Started** |
| **M2 - Layer Alpha**        | Week 10         | Multi-layer document with GPU-composited blend modes and undo/redo | **Not Started** |
| **M3 - Vector Alpha**       | Week 20         | Pen tool, shape tools, boolean ops, stroke/fill, SVG round-trip    | **Not Started** |
| **M4 - Unified Alpha**      | Week 28         | Mixed raster+vector document, PSD import, adjustment layers, text  | **Not Started** |
| **M5 - Feature Complete**   | Week 42         | All Phase 0-4 features implemented and unit tested                 | **Not Started** |
| **M6 - Release Candidate**  | Week 48         | Signed/notarized installers on all 3 platforms, 0 P0 bugs          | **Not Started** |
| **M7 - v1.0 Release**       | Week 50         | Public release: download page, release notes, plugin SDK docs      | **Not Started** |

# **6\. Risk Register**

| **Risk**                                                                  | **Likelihood** | **Impact** | **Mitigation**                                                                                                                                             |
| ------------------------------------------------------------------------- | -------------- | ---------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| PSD format complexity - undocumented fields causing import corruption     | **High**       | **Medium** | Scope PSD import to common structures only in v1.0. Use Photopea and open-source parsers as reference. Add regression tests with real PSD files.           |
| GPU abstraction (wgpu) missing platform-specific features for blend modes | **Medium**     | **High**   | Prototype all blend mode shaders in WGSL early in Phase 1. Identify gaps before committing to wgpu. Fallback: CPU path for missing blend modes.            |
| Qt6 licensing cost at commercial scale (LGPL vs commercial license)       | **Medium**     | **Medium** | Ensure dynamic linking to Qt6 for LGPL compliance. Budget for Qt commercial license if static linking is required.                                         |
| Apple notarization rejection due to hardened runtime incompatibility      | **Low**        | **High**   | Set up notarization CI pipeline in Phase 0. Test code-signing with a stub app before Phase 1. Review hardened runtime entitlements for GPU access.         |
| Performance regression: tile compositing too slow for large canvases      | **Medium**     | **High**   | Establish 16ms frame time budget as CI benchmark in Phase 1. Profile with Instruments (macOS), PIX (Windows), RenderDoc (Linux) at each phase boundary.    |
| Text rendering quality gap vs commercial apps (sub-pixel hinting, CJK)    | **Medium**     | **Medium** | Use HarfBuzz + FreeType stack (same as Chrome/Firefox). Enable sub-pixel LCD rendering on Windows and Linux. Test CJK with native speakers in beta.        |
| Team knowledge gap in GPU shader programming                              | **High**       | **High**   | Allocate 2 weeks in Phase 0 for GPU shader training. Hire or contract a graphics programmer with WGSL experience. Use Skia as CPU fallback during ramp-up. |
| Scope creep delaying v1.0 release                                         | **High**       | **Medium** | Strict feature freeze after Phase 0 enforced by PM. All new requests logged for v1.1+. Weekly milestone reviews with go/no-go decision gates.              |

# **7\. Team & Responsibilities**

| **Role**                     | **Count** | **Responsibilities**                                                                  |
| ---------------------------- | --------- | ------------------------------------------------------------------------------------- |
| **Engineering Lead**         | **1**     | Architecture decisions, code review, CI/CD, Phase 0 delivery, cross-team coordination |
| **Sr C++ Engineer (Raster)** | **2**     | Tile manager, brush engine, layer compositor, GPU blend mode shaders (Phases 1-2)     |
| **Sr C++ Engineer (Vector)** | **2**     | Scene graph, Bezier engine, boolean ops, text pipeline, SVG I/O (Phase 3)             |
| **Graphics Programmer**      | **1**     | wgpu pipeline, WGSL shaders, GPU memory management, performance optimization          |
| **Qt UI Engineer**           | **1**     | Qt6 shell, panels, dialogs, HiDPI, accessibility, engine layer integration            |
| **QA Engineer**              | **1**     | Test plan, regression suite, cross-platform matrix testing, bug triage                |
| **Product Manager**          | **1**     | Roadmap, milestone reviews, scope management, stakeholder communication               |

**Estimated Timeline**

Total: 50 weeks (~12 months) with the team above. Phases 1-3 can be partially parallelized between raster and vector teams after Phase 0 is complete. The critical path runs through the unified document model in Phase 4.

# **8\. Definition of Done**

A feature is considered done when all of the following criteria are met:

- Code compiles without warnings on macOS, Windows, and Linux in Release mode
- Unit tests written for all non-UI logic, achieving ≥80% line coverage for the module
- Integration test covers the feature end-to-end (open → modify → save → reopen → verify)
- Manually tested on all three target operating systems
- No P0 (crash) or P1 (data loss) bugs open against the feature
- Code has passed peer review by at least one other engineer
- CI build is green - all tests pass on all three platform runners
- Performance: feature does not regress canvas frame time benchmark by more than 5%
- Public API documented with Doxygen comments; user-facing feature described in changelog

## **8.1 Release Criteria (v1.0)**

- All Phase 0-5 features done per the definition above
- Zero P0 bugs, zero P1 bugs open at time of release
- macOS installer notarized, passes Gatekeeper on clean macOS 13 and macOS 14
- Windows installer code-signed, passes Windows App Certification Kit
- Linux AppImage runs on Ubuntu 22.04 LTS without additional dependencies
- 60fps canvas at 4K (3840×2160) with 20 raster layers on recommended hardware
- Minimum 20 beta users completed a 2-week structured testing period

# **Appendix A - File Format Support Matrix**

| **Format**           | **Read**      | **Write**   | **Round-trip** | **Notes**                                       |
| -------------------- | ------------- | ----------- | -------------- | ----------------------------------------------- |
| **.canvas (native)** | **Yes**       | **Yes**     | **Full**       | Custom binary; all layers, history, vector data |
| **PSD (Photoshop)**  | **Partial**   | **Partial** | **Partial**    | Common structures; smart filters unsupported    |
| **SVG 1.1**          | **Yes**       | **Yes**     | **Full**       | Vector layers only; raster embedded as data URI |
| **PDF/X-1a**         | **No**        | **Yes**     | **N/A**        | Flattened export for print workflows            |
| **PNG (8/16-bit)**   | **Yes**       | **Yes**     | **Full**       | Alpha channel and ICC profile metadata          |
| **JPEG**             | **Yes**       | **Yes**     | **Lossy**      | EXIF metadata preserved; CMYK JPEG supported    |
| **TIFF**             | **Yes**       | **Yes**     | **Full**       | 16-bit, 32-bit float, CMYK, multi-page          |
| **WebP**             | **Yes**       | **Yes**     | **Full**       | Lossy and lossless, alpha channel               |
| **AVIF**             | **Yes**       | **Yes**     | **Full**       | HDR, wide color gamut, alpha                    |
| **OpenEXR**          | **Yes**       | **Yes**     | **Full**       | 16-bit half and 32-bit float, multi-channel     |
| **GIF**              | **Read only** | **No**      | **N/A**        | Import only; animated GIF export planned v1.1   |

# **Appendix B - Recommended Development Hardware**

- macOS: Apple Silicon M2 MacBook Pro 14" (16GB RAM) - primary platform for Metal backend
- Windows: AMD Ryzen 9 7900X + RTX 4070 (16GB RAM) - Direct3D 12 and Windows installer QA
- Linux: Ubuntu 22.04 LTS + AMD Radeon RX 6700 XT (16GB RAM) - Vulkan testing, AppImage QA
- QA matrix also includes: Intel integrated graphics (UHD 770), older GPUs (GTX 1060), and VMs

---

# **Appendix C - Implementation Checklist**

*Derived from the current codebase state. Each item maps to a concrete file or code change required to complete the application. Check off as work is merged.*

---

## **Pre-Phase 0: Fix Existing Scaffold**

These blockers exist in the current committed files and must be resolved before any build succeeds.

### Build System Fixes
- [x] Create `cmake/` directory and move `Version.h.in` into it (fixes path mismatch in `CMakeLists.txt:54`)
- [x] Move `Packaging.cmake` into `cmake/` (fixes `include(cmake/Packaging.cmake)` at `CMakeLists.txt:229`)
- [x] Add `README.md` (stub is sufficient — unblocks CPack `CPACK_PACKAGE_DESCRIPTION_FILE`)
- [x] Add `LICENSE` file (MIT — unblocks CPack `CPACK_RESOURCE_FILE_LICENSE`)
- [x] Stub all five `src/` subdirectories with `CMakeLists.txt` files: `src/platform/`, `src/core/`, `src/engine/`, `src/ui/`, `src/app/`
- [x] Fix ASan flags for MSVC: MSVC block now uses `/fsanitize=address`; Clang/GCC block unchanged
- [x] Parameterize hardcoded placeholder URLs in `Packaging.cmake` (`DEXILATE_HOMEPAGE_URL`, `DEXILATE_SUPPORT_EMAIL` CMake variables)
- [x] Fix `CMakePresets.json` default test preset — now uses `ci` (platform-agnostic); per-platform test presets added

### CI Fixes
- [x] Gate SPDX header check behind `if [ -d src ]` — no longer fails before `src/` exists
- [x] Add test log artifact upload on failure: `build/*/Testing/Temporary/`
- [x] Add CodeQL workflow (`.github/workflows/codeql.yml`) — configured with `security-and-quality` queries
- [x] Parameterize Qt version (`6.7.2`) as workflow-level `env.QT_VERSION` var

### PAL Interface Completions
- [x] Create `src/platform/include/IFileSystem.h` — file dialogs, recent files, atomic writes, temp/documents/appdata dirs
- [x] Create `src/platform/include/IClipboard.h` — RGBA image, SVG path, and text copy/paste with dual-format SVG
- [x] Add thread-safety contract to `IWindow.h` — all methods documented; `present()` and `gpuSurfaceHandle()` explicitly GPU-thread safe
- [x] Resolve Ctrl/Cmd/Meta ambiguity in `InputEvent.h` — Ctrl = physical Ctrl always; Meta = Cmd on macOS, Windows key on Win/Linux; cross-platform shortcut pattern documented
- [x] Add pressure range doc comment to `StylusEvent::pressure` and `TouchPoint::pressure` (`///< [0.0, 1.0]`)
- [x] Add missing key codes to `KeyCode` enum: `PrintScreen`, `ScrollLock`, `Pause`, `Menu`, full numpad, F13–F20
- [x] Add vsync/present mode control to `IWindow` interface (`setPresentMode(PresentMode)` + `PresentMode` enum)
- [x] Make Wayland optional (not `REQUIRED`) in `src/platform/CMakeLists.txt` — X11 is default; Wayland enabled when found

---

## **Phase 0 — Toolchain & Foundation**

*Goal: GPU-rendered white canvas window opens on all three platforms from a CI build.*

### Directory Structure
- [x] Create full `src/` tree: `platform/`, `core/`, `engine/`, `ui/`, `app/` each with `CMakeLists.txt`
- [x] Create `src/platform/include/` for all PAL headers
- [x] Create `src/platform/src/macos/`, `src/platform/src/windows/`, `src/platform/src/linux/`
- [x] Add `CONTRIBUTING.md` documenting: Qt6 install, `Qt6_DIR` env var, CMake preset usage

### Dependency Integration (FetchContent)
- [x] Add wgpu-native 0.20 via `cmake/FetchWgpuNative.cmake` — prebuilt binary per platform, `wgpu::native` imported target; macOS universal binary lipo'd at configure time
- [x] Add Skia via `cmake/FetchSkia.cmake` — `DEXILATE_ENABLE_SKIA=OFF` for Phase 0 (stub `skia::skia` target); three integration paths documented (build-from-source/vcpkg/prebuilt) for Phase 1
- [x] Add Catch2 v3.6.0 via `FetchContent` for unit testing
- [x] Add GLM 1.0.1 via `FetchContent`
- [x] Add Clipper2 1.3.0 via `FetchContent`
- [x] Fix nlohmann/json 3.11.3 — switched from broken single-header URL to `GIT_REPOSITORY`; provides proper `nlohmann_json::nlohmann_json` INTERFACE target
- [x] Qt6 6.7 `find_package` in CMakeLists.txt; CI runner caching via `jurplel/install-qt-action` in `ci.yml`; `Vulkan` package added to Linux platform block for wgpu-native

### Platform Abstraction Layer — macOS
- [x] `src/platform/src/macos/MacWindow.mm` — `NSWindow` + `CAMetalLayer`; Y-flip, stylus, modifier keys, `pollEvents` drain, GPU-thread-safe `gpuSurfaceHandle()`
- [x] `src/platform/src/macos/MacFileSystem.mm` — `NSOpenPanel`/`NSSavePanel` (UTType on macOS 12+), atomic writes via `rename(2)`, recent files via `NSUserDefaults`
- [x] `src/platform/src/macos/MacClipboard.mm` — `NSPasteboard` RGBA image (CGImage), text, dual-format SVG path (`com.dexilate.svg-path` + plain string)
- [x] `src/platform/src/Platform.cpp` — PAL lifecycle (`init`/`shutdown`/`isInitialised`) with `std::atomic<bool>`
- [x] `src/platform/include/dexilate/platform/` — all headers relocated to correct namespace-qualified subpath; `CoreGraphics` framework added to macOS link libs

### Platform Abstraction Layer — Windows
- [x] `src/platform/src/windows/Win32Window.cpp` — `HWND` + DXGI surface; per-monitor-v2 DPI, PeekMessage drain, VK_ → KeyCode, UTF-16 surrogate pairs, fullscreen toggle
- [x] `src/platform/src/windows/Win32FileSystem.cpp` — `IFileOpenDialog`/`IFileSaveDialog` (WRL ComPtr), NTFS atomic rename, registry recent-files (REG_MULTI_SZ)
- [x] `src/platform/src/windows/Win32Clipboard.cpp` — `CF_DIBV5` BGRA image with alpha, `CF_UNICODETEXT`, custom registered format for SVG paths

### Platform Abstraction Layer — Linux
- [x] `src/platform/src/linux/X11Window.cpp` — Xlib + Vulkan surface; XKB keysym mapping, _NET_WM_STATE_FULLSCREEN, DPI from Xft.dpi, runtime factory (no DEXILATE_WAYLAND)
- [x] `src/platform/src/linux/WaylandWindow.cpp` — wl_compositor + wl_surface; xkbcommon; runtime picks Wayland when WAYLAND_DISPLAY set, X11 fallback; guarded by #ifdef DEXILATE_WAYLAND
- [x] `src/platform/src/linux/LinuxFileSystem.cpp` — zenity(1) subprocess dialogs, XDG Base Directory, POSIX-atomic rename, plain-text recent files
- [x] `src/platform/src/linux/LinuxClipboard.cpp` — X11 CLIPBOARD selection with in-process cache; UTF8_STRING, custom SVG atom, TARGETS negotiation; PNG decode deferred to Phase 1

### GPU Layer — Hello Dexilate
- [x] `src/app/main.cpp` — application entry point; creates `IWindow`, initialises wgpu, runs clear-pass render loop
- [x] `src/app/GpuContext.h` / `GpuContext.cpp` — RAII wrapper: adapter + device + queue via synchronous wgpu-native callbacks
- [x] Render loop: `pollEvents` → reconfigure on resize → `wgpuSurfaceGetCurrentTexture` → clear pass (#F0F0F0) → `wgpuSurfacePresent`
- [ ] `src/app/DexilateApp.cpp` / `DexilateApp.h` — app lifecycle class (deferred to Phase 1 when Qt6 shell is added)
- [ ] `src/engine/gpu/Surface.cpp` — dedicated surface wrapper (deferred; surface config inlined in main.cpp for Phase 0)
- [ ] `assets/shaders/clear.wgsl` — not needed; wgpu `WGPULoadOp_Clear` requires no shader

### Testing
- [x] `tests/platform/test_window.cpp` — Catch2 integration tests for `IWindow` lifecycle; `SKIP` guards for headless CI
- [x] `tests/platform/test_input_event.cpp` — headless unit tests: `Modifiers` bitmask, `KeyCode` ASCII values, `InputEvent` variant + `std::visit`
- [x] Wire `tests/` into CMake — `add_dexilate_test` helper; `catch_discover_tests` CTest registration
- [ ] CI: confirm CTest runs and reports pass/fail per platform

### Milestone Gate: M0
- [ ] White canvas window opens on macOS, Windows, and Linux from a single CI build artifact
- [ ] All Catch2 PAL tests pass on all three runners
- [ ] CI is fully green (build + test + SPDX check + artifact upload)

---

## **Phase 1 — Raster Core**

*Goal: Open a PNG, paint with a pressure-sensitive brush, save PNG.*

### Core Data Structures
- [ ] `src/core/Tile.h` / `Tile.cpp` — 256×256 pixel buffer, CPU-side, 8-bit and 16-bit variants
- [ ] `src/engine/raster/TileManager.cpp` — tile grid, dirty region tracking, LRU eviction
- [ ] `src/engine/document/Document.cpp` — single raster layer document model (width, height, bit depth)
- [ ] `src/engine/document/Layer.h` — base layer type; `RasterLayer` subclass with tile grid

### Image Codecs
- [ ] Add libpng 1.6 via `FetchContent`
- [ ] Add libjpeg-turbo 3.x via `FetchContent`
- [ ] `src/core/codecs/ICodec.h` — common decode/encode interface for all formats
- [ ] `src/core/codecs/PngCodec.cpp` — PNG read (into `RasterLayer`) and write (from `RasterLayer`)

### Brush Engine
- [ ] `src/engine/raster/BrushEngine.cpp` — round brush: dab placement, spacing, pressure→opacity/size mapping
- [ ] `src/engine/raster/Eraser.cpp` — eraser composites onto alpha channel
- [ ] `src/engine/raster/InputSampler.cpp` — converts stylus `InputEvent` stream to brush dab sequence

### View Transform
- [ ] `src/engine/viewport/ViewTransform.cpp` — affine matrix: pan (click-drag), zoom (scroll 0.1×–64×), rotate
- [ ] Keyboard shortcuts: Space+drag = pan, Ctrl+scroll = zoom, R = rotate

### GPU Upload
- [ ] `src/engine/gpu/TileUploader.cpp` — dirty tile CPU→GPU texture upload per frame
- [ ] `assets/shaders/composite.wgsl` — single-layer tile quad compositing shader
- [ ] Frame loop: collect dirty tiles → upload → composite pass → present

### Qt6 Shell (Phase 1 scope)
- [ ] `src/ui/MainWindow.cpp` — `QMainWindow` subclass with menu bar: File → New, Open, Save, Save As, Quit
- [ ] `src/ui/DexilateWidget.cpp` — `QWidget` embedding raw GPU surface via `QWindow::fromWinId`
- [ ] `src/ui/ToolOptionsBar.cpp` — brush size and opacity sliders
- [ ] `src/ui/StatusBar.cpp` — cursor coordinates, zoom level, document dimensions

### Tests
- [ ] `tests/core/test_tile.cpp` — tile allocation, dirty marking, pixel read/write
- [ ] `tests/core/test_png_codec.cpp` — round-trip: write known pixels → read back → assert equal
- [ ] `tests/engine/test_brush.cpp` — dab placement at known pressure values

### Milestone Gate: M1
- [ ] Open PNG file, paint strokes with pressure-sensitive brush, save PNG — all on all three platforms

---

## **Phase 2 — Layer System**

*Goal: Multi-layer document with GPU blend modes, opacity, groups, adjustment layers, history.*

### Layer Model
- [ ] `src/engine/document/LayerTree.cpp` — heterogeneous layer tree: add, delete, reorder, group
- [ ] `src/engine/document/GroupLayer.cpp` — clipping mask, pass-through blend mode
- [ ] `src/engine/document/AdjustmentLayer.cpp` — base class; subclasses: Curves, Levels, Hue/Saturation, Color Balance, Brightness/Contrast
- [ ] Per-layer: opacity (`float`), blend mode (`enum BlendMode`), visibility, lock state

### GPU Compositor
- [ ] `assets/shaders/blend_modes.wgsl` — all 20 ISO blend modes as WGSL functions (Normal, Multiply, Screen, Overlay, etc.)
- [ ] `src/engine/gpu/Compositor.cpp` — iterate layer stack bottom-to-top, composite dirty tiles per frame
- [ ] GPU ping-pong texture buffers for intermediate compositing results

### History / Undo
- [ ] `src/engine/document/Command.h` — command pattern base interface (`execute`, `undo`)
- [ ] `src/engine/document/HistoryManager.cpp` — undo/redo stack, memory budget via tile diffing
- [ ] Implement commands: `PaintCommand`, `EraseCommand`, `MoveLayerCommand`, `AddLayerCommand`, `DeleteLayerCommand`

### Selection
- [ ] `src/engine/raster/Selection.cpp` — 1-bit selection mask at document resolution
- [ ] `src/engine/tools/RectMarqueeTool.cpp`
- [ ] `src/engine/tools/LassoTool.cpp`
- [ ] `src/engine/tools/QuickSelectionTool.cpp` — flood-fill based selection

### Clipboard
- [ ] Wire `IClipboard` into copy (selection→clipboard) and paste (clipboard→new layer) operations

### Qt6 Shell (Phase 2 additions)
- [ ] `src/ui/LayerPanel.cpp` — layer list widget: thumbnail, visibility eye, lock, blend mode dropdown, opacity slider
- [ ] `src/ui/HistoryPanel.cpp` — scrollable undo history list
- [ ] Selection tool buttons in toolbar
- [ ] Edit menu: Undo, Redo, Copy, Paste, Cut

### Tests
- [ ] `tests/engine/test_compositor.cpp` — composite known pixel values through each blend mode, compare against reference
- [ ] `tests/engine/test_history.cpp` — execute N commands, undo all, assert document state matches initial

### Milestone Gate: M2
- [ ] Multi-layer document composited in real time with all 20 blend modes, undo/redo, clipboard working on all platforms

---

## **Phase 3 — Vector Engine**

*Goal: Pen tool, shape tools, boolean ops, text, SVG round-trip.*

### Scene Graph
- [ ] `src/engine/vector/SceneGraph.cpp` — node tree with affine transforms and z-order
- [ ] `src/engine/vector/Node.h` — base node: `PathNode`, `GroupNode`, `TextNode`, `PlacedImageNode`
- [ ] `src/engine/vector/Transform.cpp` — 2D affine matrix operations

### Path & Bezier
- [ ] `src/engine/vector/Path.cpp` — contour list: `moveTo`, `lineTo`, `curveTo`, `close`; winding rule
- [ ] `src/engine/vector/Stroker.cpp` — path stroker: miter/round/bevel joins, dashed patterns
- [ ] `src/engine/vector/Fill.cpp` — solid color, linear gradient, radial gradient, pattern fill
- [ ] Add Clipper2 1.3: `union`, `difference`, `intersect`, `XOR` boolean operations on paths

### Tools
- [ ] `src/engine/tools/PenTool.cpp` — click to add anchor, drag for Bezier handles, close path
- [ ] `src/engine/tools/NodeEditor.cpp` — select/move/add/delete anchors, convert corner↔smooth
- [ ] `src/engine/tools/RectangleTool.cpp`
- [ ] `src/engine/tools/EllipseTool.cpp`
- [ ] `src/engine/tools/PolygonTool.cpp` (configurable sides)
- [ ] `src/engine/tools/StarTool.cpp` (configurable points and inner radius)
- [ ] `src/engine/tools/LineTool.cpp`

### Text
- [ ] Add HarfBuzz 8.x and FreeType 2.13 via `FetchContent`
- [ ] `src/core/text/FontManager.cpp` — enumerate system fonts, load face via FreeType
- [ ] `src/core/text/TextShaper.cpp` — HarfBuzz shaping pipeline: runs → glyphs → positions
- [ ] `src/engine/vector/TextFrame.cpp` — area text, point text, text on path (arc-length parameterization)

### SVG I/O
- [ ] `src/core/codecs/SvgImporter.cpp` — parse SVG 1.1 into scene graph nodes (paths, groups, text, images)
- [ ] `src/core/codecs/SvgExporter.cpp` — serialize scene graph to clean SVG 1.1

### Vector Rasterization
- [ ] `src/engine/vector/VectorRasterizer.cpp` — rasterize scene graph nodes to dirty tiles on demand using Skia

### Qt6 Shell (Phase 3 additions)
- [ ] Vector tool buttons in toolbar (Pen, Node, Rectangle, Ellipse, etc.)
- [ ] `src/ui/FontPicker.cpp` — font family, size, weight, leading, tracking, kerning controls
- [ ] `src/ui/StrokeFillPanel.cpp` — stroke/fill color picker, gradient editor
- [ ] SVG import/export in File menu

### Tests
- [ ] `tests/engine/test_path.cpp` — contour operations, winding, stroker output
- [ ] `tests/engine/test_boolean_ops.cpp` — union/difference/intersect against known polygon results
- [ ] `tests/core/test_svg_roundtrip.cpp` — import SVG → export SVG → compare path data

### Milestone Gate: M3
- [ ] Pen tool, shape tools, boolean ops, stroke/fill, SVG round-trip working on all platforms

---

## **Phase 4 — Unified Document**

*Goal: Mixed raster+vector, PSD import/export, smart objects, color management, PDF export.*

### Mixed Layer Tree
- [ ] `Document` layer tree accepts both `RasterLayer` and vector `Node` types as siblings
- [ ] `src/engine/document/SmartObject.cpp` — embedded vector doc inside raster layer, edit non-destructively
- [ ] `src/engine/document/VectorLayer.cpp` — vector scene graph node as a document layer

### PSD I/O
- [ ] `src/core/codecs/PsdImporter.cpp` — parse PSD layer structure, raster data, adjustment layers, text layers
- [ ] `src/core/codecs/PsdExporter.cpp` — serialize document to PSD (Photoshop-compatible)
- [ ] Regression test suite: import real PSD files, compare rasterized output against reference renders

### Additional Codecs
- [ ] Add libtiff 4.6 via `FetchContent`; `src/core/codecs/TiffCodec.cpp`
- [ ] Add libwebp + libavif via `FetchContent`; `WebpCodec.cpp`, `AvifCodec.cpp`
- [ ] Add OpenEXR 3.2 via `FetchContent`; `ExrCodec.cpp`
- [ ] Add libjpeg-turbo CMYK read path; `JpegCodec.cpp` (EXIF metadata preservation)

### PDF Export
- [ ] Add Cairo PDF backend; `src/core/codecs/PdfExporter.cpp` — flatten document to PDF/X-1a

### Color Management
- [ ] Add LittleCMS 2.16 via `FetchContent`
- [ ] `src/core/color/ColorManager.cpp` — ICC profile assign/convert per layer and document
- [ ] `src/engine/viewport/SoftProof.cpp` — on-screen preview in target output color space
- [ ] Display ICC profile detection per monitor (macOS `CGColorSpace`, Windows ICM, Linux colord)

### Qt6 Shell (Phase 4 additions)
- [ ] File menu: PSD import/export, PDF export, TIFF/WebP/AVIF/EXR open/save
- [ ] `src/ui/ColorProfilePanel.cpp` — assign/convert color profile dialog
- [ ] Soft-proof toggle in View menu

### Tests
- [ ] `tests/core/test_psd_import.cpp` — known PSD files produce correct layer count and pixel values
- [ ] `tests/core/test_color_management.cpp` — sRGB→Display P3 round-trip within tolerance
- [ ] `tests/engine/test_smart_object.cpp` — edit embedded vector doc, assert raster layer updates

### Milestone Gate: M4
- [ ] Mixed raster+vector document, PSD import, adjustment layers, text — working on all platforms

---

## **Phase 5 — Polish & Release**

*Goal: GPU filter pipeline, plugin API, packaging, QA, v1.0 release.*

### GPU Filter Pipeline
- [ ] `assets/shaders/filters/gaussian_blur.wgsl` — separable Gaussian blur as compute shader
- [ ] `assets/shaders/filters/motion_blur.wgsl`
- [ ] `assets/shaders/filters/radial_blur.wgsl`
- [ ] `assets/shaders/filters/sharpen.wgsl`
- [ ] `assets/shaders/filters/noise.wgsl`
- [ ] `src/engine/raster/FilterStack.cpp` — non-destructive smart filter layer: stores params, re-evaluates on demand

### Plugin API
- [ ] `src/app/plugin/PluginAPI.h` — public C ABI header (extern "C", stable types only)
- [ ] `src/app/plugin/PluginLoader.cpp` — `IPlatformPlugin` dynamic library loading per OS
- [ ] Write one sample plugin (e.g., Invert Colors filter)
- [ ] Publish plugin SDK: `PluginAPI.h` + documentation + sample plugin + CMakeLists

### Performance
- [ ] Establish 16ms frame time CI benchmark (`tests/perf/bench_compositor.cpp`)
- [ ] Profile and tune tile eviction: LRU policy, VRAM budget enforcement
- [ ] Profile brush dab throughput: target ≥1000 dabs/sec at full resolution

### Accessibility
- [ ] Add `QAccessible` labels to all toolbar buttons, panels, and dialogs
- [ ] Test with VoiceOver (macOS), Narrator (Windows), Orca (Linux)

### Packaging & Signing
- [ ] macOS: code-signing entitlements file (`Dexilate.entitlements`) for hardened runtime + GPU access
- [ ] macOS: automated notarization in CI via `notarytool` using GitHub Actions secrets
- [ ] macOS: `.dmg` with drag-to-Applications layout via CPack
- [ ] Windows: EV certificate code signing step in CI
- [ ] Windows: NSIS installer with file associations (`.canvas`, `.psd` open-with)
- [ ] Linux: AppImage build via `linuxdeployqt`; `.deb` for Ubuntu PPA; Flatpak manifest

### QA
- [ ] Cross-platform regression suite: 50+ test documents exercising every feature
- [ ] 60fps benchmark at 4K / 20 raster layers on recommended hardware (automated)
- [ ] Beta program: 20+ users on structured 2-week testing plan
- [ ] P0/P1 bug count = 0 before release branch cut

### Release
- [ ] `CHANGELOG.md` with all user-facing changes from Phase 0–5
- [ ] Doxygen documentation generated and published
- [ ] Plugin SDK zip published alongside installers
- [ ] GitHub release with signed installer artifacts for all three platforms

### Milestone Gates: M5 → M7
- [ ] **M5 (Week 42)**: All Phase 0–4 features implemented and unit tested, CI green
- [ ] **M6 (Week 48)**: Signed/notarized installers on all 3 platforms, zero P0 bugs
- [ ] **M7 (Week 50)**: Public v1.0 release — download page, release notes, plugin SDK published

---

*Last updated: May 2026 · Items reflect codebase state as of initial scaffold commit*