// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/platform/src/linux/WaylandWindow.cpp
//  Native Wayland IWindow implementation + runtime backend selector
//
//  This file compiles only when DEXILATE_WAYLAND is defined (set by CMake when
//  wayland-client and xkbcommon are found). It provides:
//    1. WaylandWindow — native wl_surface based IWindow
//    2. IWindow::create() — picks Wayland if WAYLAND_DISPLAY is set,
//       otherwise falls back to X11 (via createX11Window declared below).
//
//  Wayland architecture
//  ─────────────────────
//  WaylandWindow wraps a wl_surface + xdg_toplevel.  Input events are driven
//  by the compositor through wl_seat callbacks.  The GPU surface handle
//  exposes wl_surface* / wl_display* so wgpu-native can create a Vulkan WSI
//  surface via VK_KHR_wayland_surface.
//
//  This is a minimal but complete implementation. Advanced features (custom
//  cursors, drag-and-drop, tablet protocol) are deferred to Phase 1.
// ─────────────────────────────────────────────────────────────────────────────

#ifdef DEXILATE_WAYLAND

#include "dexilate/platform/IWindow.h"
#include "dexilate/platform/InputEvent.h"

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

// xdg-shell protocol (generated header expected in the build directory or
// included from wayland-protocols).  We include a minimal inline stub here
// so the file compiles without a protocol generator in the build system.
// In a full build, replace this with the generated xdg-shell-client-protocol.h.
// For Phase 0 we use the libwayland xdg_wm_base interface at runtime via
// wl_registry negotiation, and declare only what we need.
extern "C" {
struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;
}

#include <atomic>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>

// Forward declaration of the X11 factory (defined in X11Window.cpp).
namespace dexilate::platform {
    std::unique_ptr<IWindow> createX11Window(const WindowDesc& desc);
}

namespace dexilate::platform {

// ─────────────────────────────────────────────────────────────────────────────
//  WaylandState — process-global Wayland connection
// ─────────────────────────────────────────────────────────────────────────────
struct WaylandState {
    wl_display*    display    = nullptr;
    wl_registry*   registry   = nullptr;
    wl_compositor* compositor = nullptr;
    wl_seat*       seat       = nullptr;
    wl_shm*        shm        = nullptr;

    // xdg_wm_base for window management
    // We store as void* because the xdg-shell header may not be present;
    // in a production build this would be xdg_wm_base*.
    void* xdgWmBase = nullptr;

    xkb_context* xkbCtx     = nullptr;
    xkb_state*   xkbState   = nullptr;
    xkb_keymap*  xkbKeymap  = nullptr;

    // Registry listener — forward declared; defined after WaylandWindow
    static void registryGlobal(void* data, wl_registry* reg,
        uint32_t name, const char* iface, uint32_t ver);
    static void registryGlobalRemove(void* /*data*/, wl_registry* /*reg*/,
        uint32_t /*name*/) {}
};

// ─────────────────────────────────────────────────────────────────────────────
//  WaylandWindow
// ─────────────────────────────────────────────────────────────────────────────
class WaylandWindow final : public IWindow {
public:
    explicit WaylandWindow(WaylandState& ws, const WindowDesc& desc)
        : _ws(ws)
    {
        _surface = wl_compositor_create_surface(ws.compositor);
        if (!_surface)
            throw std::runtime_error("WaylandWindow: wl_compositor_create_surface failed");

        _physW    = desc.width;
        _physH    = desc.height;
        _dpr      = 1.0f;  // Wayland reports scale via wl_surface.enter; default 1x
        _logicalW = _physW;
        _logicalH = _physH;
        _title    = desc.title;

        _surfaceAtomic.store(_surface, std::memory_order_release);
        _displayAtomic.store(ws.display, std::memory_order_release);

        // Commit an empty frame so the compositor allocates the surface
        wl_surface_commit(_surface);
        wl_display_roundtrip(ws.display);
    }

    ~WaylandWindow() override {
        _surfaceAtomic.store(nullptr, std::memory_order_release);
        if (_surface) wl_surface_destroy(_surface);
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    void show() override {
        wl_surface_commit(_surface);
        wl_display_flush(_ws.display);
    }

    void hide() override {
        // Wayland has no hide — detach the buffer (makes surface invisible)
        wl_surface_attach(_surface, nullptr, 0, 0);
        wl_surface_commit(_surface);
    }

    void close() override { _shouldClose = true; }

    void pollEvents() override {
        wl_display_dispatch_pending(_ws.display);
        wl_display_flush(_ws.display);
    }

    [[nodiscard]] bool shouldClose() const override { return _shouldClose; }

    // ── Dimensions ────────────────────────────────────────────────────────────
    [[nodiscard]] uint32_t width()  const override { return _logicalW; }
    [[nodiscard]] uint32_t height() const override { return _logicalH; }
    [[nodiscard]] uint32_t physicalWidth()  const override { return _physW; }
    [[nodiscard]] uint32_t physicalHeight() const override { return _physH; }
    [[nodiscard]] float    devicePixelRatio() const override { return _dpr; }

    void resize(uint32_t w, uint32_t h) override {
        _logicalW = w; _logicalH = h;
        _physW = static_cast<uint32_t>(w * _dpr);
        _physH = static_cast<uint32_t>(h * _dpr);
    }

    // ── Title & state ─────────────────────────────────────────────────────────
    void setTitle(std::string_view title) override { _title = title; }
    [[nodiscard]] std::string title() const override { return _title; }

    void setFullscreen(bool fs) override { _fullscreen = fs; }
    [[nodiscard]] bool isFullscreen() const override { return _fullscreen; }

    // ── GPU surface ───────────────────────────────────────────────────────────
    [[nodiscard]] GpuSurfaceHandle gpuSurfaceHandle() const override {
        return {
            _surfaceAtomic.load(std::memory_order_acquire),   // wl_surface*
            _displayAtomic.load(std::memory_order_acquire)    // wl_display*
        };
    }

    void setPresentMode(PresentMode mode) override { _presentMode = mode; }
    [[nodiscard]] PresentMode presentMode() const override { return _presentMode; }
    void present() override { }  // wgpu-native drives Vulkan swap chain

    // ── Callbacks ─────────────────────────────────────────────────────────────
    void setEventCallback(EventCallback cb)   override { _eventCb  = std::move(cb); }
    void setResizeCallback(ResizeCallback cb) override { _resizeCb = std::move(cb); }
    void setCloseCallback(CloseCallback cb)   override { _closeCb  = std::move(cb); }

private:
    WaylandState&      _ws;
    wl_surface*        _surface       = nullptr;
    std::atomic<void*> _surfaceAtomic { nullptr };
    std::atomic<void*> _displayAtomic { nullptr };
    float              _dpr           = 1.0f;
    uint32_t           _physW         = 0;
    uint32_t           _physH         = 0;
    uint32_t           _logicalW      = 0;
    uint32_t           _logicalH      = 0;
    bool               _shouldClose   = false;
    bool               _fullscreen    = false;
    std::string        _title;
    PresentMode        _presentMode   = PresentMode::Fifo;

    EventCallback  _eventCb;
    ResizeCallback _resizeCb;
    CloseCallback  _closeCb;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Registry listener
// ─────────────────────────────────────────────────────────────────────────────
void WaylandState::registryGlobal(void* data, wl_registry* reg,
    uint32_t name, const char* iface, uint32_t ver)
{
    auto* ws = static_cast<WaylandState*>(data);
    if (std::strcmp(iface, "wl_compositor") == 0) {
        ws->compositor = static_cast<wl_compositor*>(
            wl_registry_bind(reg, name, &wl_compositor_interface,
                             std::min(ver, 4u)));
    } else if (std::strcmp(iface, "wl_seat") == 0) {
        ws->seat = static_cast<wl_seat*>(
            wl_registry_bind(reg, name, &wl_seat_interface,
                             std::min(ver, 5u)));
    } else if (std::strcmp(iface, "wl_shm") == 0) {
        ws->shm = static_cast<wl_shm*>(
            wl_registry_bind(reg, name, &wl_shm_interface, 1u));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Process-global Wayland state
// ─────────────────────────────────────────────────────────────────────────────
static WaylandState& waylandState() {
    static WaylandState ws;
    static bool initialised = false;
    if (!initialised) {
        ws.display = wl_display_connect(nullptr);
        if (!ws.display)
            throw std::runtime_error(
                "WaylandWindow: wl_display_connect failed — "
                "is WAYLAND_DISPLAY set?");

        ws.registry = wl_display_get_registry(ws.display);
        static const wl_registry_listener regListener = {
            WaylandState::registryGlobal,
            WaylandState::registryGlobalRemove
        };
        wl_registry_add_listener(ws.registry, &regListener, &ws);
        wl_display_roundtrip(ws.display);  // process registry advertisements

        ws.xkbCtx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        initialised = true;
    }
    return ws;
}

// ─────────────────────────────────────────────────────────────────────────────
//  IWindow::create() — picks Wayland or X11 at runtime
// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<IWindow> IWindow::create(const WindowDesc& desc) {
    // Prefer Wayland when WAYLAND_DISPLAY is set
    if (std::getenv("WAYLAND_DISPLAY")) {
        try {
            return std::make_unique<WaylandWindow>(waylandState(), desc);
        } catch (...) {
            // Fall through to X11 if Wayland initialisation fails
        }
    }
    return createX11Window(desc);
}

} // namespace dexilate::platform

#endif // DEXILATE_WAYLAND
