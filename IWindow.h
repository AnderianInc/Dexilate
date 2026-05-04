// SPDX-License-Identifier: MIT
// Canvas — Professional Creative Suite
// Copyright (c) 2026 Canvas Software

#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  canvas/platform/IWindow.h
//
//  Platform-agnostic window and GPU surface abstraction (Layer 2 PAL).
//
//  This interface decouples the engine and UI layers from OS-specific window
//  management. Implementations live in:
//    src/platform/macos/  — Cocoa/Metal window
//    src/platform/windows/ — Win32/D3D12 window
//    src/platform/linux/  — X11+Wayland/Vulkan window
//
//  Usage:
//    auto window = canvas::platform::IWindow::create({
//        .title  = "Canvas",
//        .width  = 1280,
//        .height = 800,
//    });
//    window->show();
//    while (!window->shouldClose()) {
//        window->pollEvents();
//        // ... render frame ...
//        window->present();
//    }
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace canvas::platform {

// ── Forward declarations ──────────────────────────────────────────────────────
struct InputEvent;

// ── Window creation parameters ────────────────────────────────────────────────
struct WindowDesc {
    std::string title   = "Canvas";
    uint32_t    width   = 1280;
    uint32_t    height  = 800;
    bool        resizable   = true;
    bool        maximized   = false;
    bool        fullscreen  = false;
    bool        hidpi       = true;    ///< Enable HiDPI / Retina support
};

// ── GPU surface handle (opaque per-platform pointer) ─────────────────────────
/// The GPU backend (wgpu/Metal/D3D12/Vulkan) receives this to create a swap
/// chain. The concrete type differs per OS:
///   macOS   → CAMetalLayer*
///   Windows → HWND
///   Linux   → (Display*, Window) pair or VkSurfaceKHR seed data
struct GpuSurfaceHandle {
    void* nativeWindowHandle = nullptr;   ///< OS window handle
    void* nativeDisplayHandle = nullptr;  ///< X11 Display* or nullptr
};

// ─────────────────────────────────────────────────────────────────────────────
//  IWindow
// ─────────────────────────────────────────────────────────────────────────────
class IWindow {
public:
    virtual ~IWindow() = default;

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    virtual void show()  = 0;
    virtual void hide()  = 0;
    virtual void close() = 0;

    /// Process pending OS events (non-blocking). Call once per frame.
    virtual void pollEvents() = 0;

    /// Returns true when the user has requested the window to close
    /// (e.g. clicked the ✕ button or pressed Cmd+Q).
    [[nodiscard]] virtual bool shouldClose() const = 0;

    // ── Dimensions ────────────────────────────────────────────────────────────
    /// Logical (CSS-pixel) dimensions — independent of device pixel ratio.
    [[nodiscard]] virtual uint32_t width()  const = 0;
    [[nodiscard]] virtual uint32_t height() const = 0;

    /// Physical pixel dimensions = logical × devicePixelRatio().
    [[nodiscard]] virtual uint32_t physicalWidth()  const = 0;
    [[nodiscard]] virtual uint32_t physicalHeight() const = 0;

    /// Retina/HiDPI scale factor (typically 1.0, 1.5, or 2.0).
    [[nodiscard]] virtual float devicePixelRatio() const = 0;

    virtual void resize(uint32_t width, uint32_t height) = 0;

    // ── Title & state ─────────────────────────────────────────────────────────
    virtual void setTitle(std::string_view title) = 0;
    [[nodiscard]] virtual std::string title() const = 0;

    virtual void setFullscreen(bool fullscreen) = 0;
    [[nodiscard]] virtual bool isFullscreen() const = 0;

    // ── GPU surface ───────────────────────────────────────────────────────────
    /// Returns the OS-native handle used by the GPU backend to create a
    /// swap chain surface. Valid after show() is called.
    [[nodiscard]] virtual GpuSurfaceHandle gpuSurfaceHandle() const = 0;

    /// Called by the GPU backend after each frame is rendered.
    virtual void present() = 0;

    // ── Input callbacks ───────────────────────────────────────────────────────
    using EventCallback = std::function<void(const InputEvent&)>;
    virtual void setEventCallback(EventCallback callback) = 0;

    using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
    virtual void setResizeCallback(ResizeCallback callback) = 0;

    using CloseCallback = std::function<void()>;
    virtual void setCloseCallback(CloseCallback callback) = 0;

    // ── Factory ───────────────────────────────────────────────────────────────
    /// Creates the platform-native window implementation.
    /// On first call, initialises any necessary platform subsystems.
    [[nodiscard]] static std::unique_ptr<IWindow> create(const WindowDesc& desc);
};

} // namespace canvas::platform
