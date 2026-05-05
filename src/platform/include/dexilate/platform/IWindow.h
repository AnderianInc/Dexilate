// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  dexilate/platform/IWindow.h
//
//  Platform-agnostic window and GPU surface abstraction (Layer 2 PAL).
//
//  Thread safety
//  ─────────────
//  ALL methods must be called from the main thread unless explicitly noted.
//  The GPU render loop runs on a dedicated thread but only calls:
//    - gpuSurfaceHandle()  — safe after show() returns (read-only, immutable)
//    - present()           — explicitly GPU-thread safe (see below)
//
//  Implementations live in:
//    src/platform/src/macos/   — Cocoa/Metal window (NSWindow + CAMetalLayer)
//    src/platform/src/windows/ — Win32/DXGI window  (HWND + swap chain)
//    src/platform/src/linux/   — X11/Wayland/Vulkan window
//
//  Usage:
//    auto window = dexilate::platform::IWindow::create({
//        .title  = "Dexilate",
//        .width  = 1280,
//        .height = 800,
//    });
//    window->show();
//    while (!window->shouldClose()) {
//        window->pollEvents();          // main thread — dispatches input callbacks
//        renderFrameOnGpuThread();      // GPU thread — calls present() at end
//    }
// ─────────────────────────────────────────────────────────────────────────────

#include "InputEvent.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace dexilate::platform {

// ── Vsync / present mode ──────────────────────────────────────────────────────
enum class PresentMode : uint8_t {
    Immediate,  ///< No vsync — lowest latency, may tear
    Fifo,       ///< Hard vsync (default) — waits for vblank, no tearing
    Mailbox,    ///< Triple-buffer — no tearing, lower latency than Fifo
};

// ── Window creation parameters ────────────────────────────────────────────────
struct WindowDesc {
    std::string title       = "Dexilate";
    uint32_t    width       = 1280;
    uint32_t    height      = 800;
    bool        resizable   = true;
    bool        maximized   = false;
    bool        fullscreen  = false;
    bool        hidpi       = true;               ///< Enable HiDPI / Retina support
    PresentMode presentMode = PresentMode::Fifo;
};

// ── GPU surface handle ────────────────────────────────────────────────────────
/// Opaque handle passed to the GPU backend (wgpu-native) to create a swap
/// chain surface. Field semantics per OS:
///   macOS   → nativeWindowHandle = CAMetalLayer*,  nativeDisplayHandle = nullptr
///   Windows → nativeWindowHandle = HWND,           nativeDisplayHandle = nullptr
///   Linux   → nativeWindowHandle = Window (XID),   nativeDisplayHandle = Display*
///             or nativeWindowHandle = wl_surface*,  nativeDisplayHandle = wl_display*
///
/// Thread safety: safe to read from any thread after show() returns.
struct GpuSurfaceHandle {
    void* nativeWindowHandle  = nullptr;  ///< OS window / layer handle
    void* nativeDisplayHandle = nullptr;  ///< X11 Display* / wl_display*, or nullptr
};

// ─────────────────────────────────────────────────────────────────────────────
//  IWindow
// ─────────────────────────────────────────────────────────────────────────────
class IWindow {
public:
    virtual ~IWindow() = default;

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    /// Main thread only.
    virtual void show()  = 0;
    virtual void hide()  = 0;
    virtual void close() = 0;

    /// Process pending OS events (non-blocking). Call once per frame from the
    /// main thread. Invokes registered callbacks synchronously before returning.
    virtual void pollEvents() = 0;

    /// True when the user has requested close (✕ button, Cmd+Q, Alt+F4, etc.).
    /// Main thread only.
    [[nodiscard]] virtual bool shouldClose() const = 0;

    // ── Dimensions ────────────────────────────────────────────────────────────
    /// Logical (CSS-pixel) dimensions — independent of device pixel ratio.
    /// Main thread only.
    [[nodiscard]] virtual uint32_t width()  const = 0;
    [[nodiscard]] virtual uint32_t height() const = 0;

    /// Physical pixel dimensions = logical × devicePixelRatio().
    /// Main thread only.
    [[nodiscard]] virtual uint32_t physicalWidth()  const = 0;
    [[nodiscard]] virtual uint32_t physicalHeight() const = 0;

    /// Retina/HiDPI scale factor (typically 1.0, 1.5, or 2.0).
    /// Main thread only.
    [[nodiscard]] virtual float devicePixelRatio() const = 0;

    /// Main thread only.
    virtual void resize(uint32_t width, uint32_t height) = 0;

    // ── Title & state ─────────────────────────────────────────────────────────
    /// Main thread only.
    virtual void setTitle(std::string_view title) = 0;
    [[nodiscard]] virtual std::string title() const = 0;

    virtual void setFullscreen(bool fullscreen) = 0;
    [[nodiscard]] virtual bool isFullscreen()   const = 0;

    // ── GPU surface ───────────────────────────────────────────────────────────
    /// Returns the OS-native handle used by the GPU backend to create a swap
    /// chain surface. Immutable after show() returns.
    /// Thread safety: safe to call from any thread after show().
    [[nodiscard]] virtual GpuSurfaceHandle gpuSurfaceHandle() const = 0;

    /// Change vsync / present mode. Takes effect on next swap chain recreation.
    /// Main thread only; call before entering the render loop or between frames.
    virtual void setPresentMode(PresentMode mode) = 0;
    [[nodiscard]] virtual PresentMode presentMode() const = 0;

    /// Signal that a rendered frame is ready to display.
    /// Thread safety: may be called from the GPU / render thread.
    virtual void present() = 0;

    // ── Input callbacks ───────────────────────────────────────────────────────
    /// All callbacks are invoked on the main thread inside pollEvents().
    using EventCallback  = std::function<void(const InputEvent&)>;
    using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
    using CloseCallback  = std::function<void()>;

    virtual void setEventCallback(EventCallback callback)   = 0;
    virtual void setResizeCallback(ResizeCallback callback) = 0;
    virtual void setCloseCallback(CloseCallback callback)   = 0;

    // ── Factory ───────────────────────────────────────────────────────────────
    /// Creates the platform-native window implementation.
    /// On first call, initialises platform subsystems (Cocoa, Win32, X11, etc.).
    /// Must be called from the main thread.
    [[nodiscard]] static std::unique_ptr<IWindow> create(const WindowDesc& desc);
};

} // namespace dexilate::platform
