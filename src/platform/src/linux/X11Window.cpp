// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/platform/src/linux/X11Window.cpp
//  IWindow implementation for Linux — Xlib + wgpu-native Vulkan surface
//
//  Architecture
//  ────────────
//  X11Window    C++ class implementing IWindow. Owns the Display* and Window
//               (XID). Translates XEvent structs to typed InputEvents.
//
//  IWindow::create()  Factory — opens Display, maps the window, returns
//                     std::unique_ptr<X11Window>.
//
//  Coordinate system
//  ──────────────────
//  X11 origin is top-left (same as Dexilate). No Y-flip needed.
//  Coordinates are in physical pixels. We divide by devicePixelRatio()
//  (inferred from Xft.dpi resource or screen DPI) to yield logical pixels.
//
//  Thread safety
//  ─────────────
//  All Xlib calls are main-thread only. gpuSurfaceHandle() may be called from
//  the GPU thread after show() returns.
//
//  Keyboard
//  ────────
//  XLookupString is used for ASCII text. XkbKeycodeToKeysym maps physical
//  keys to KeySyms, which we then translate to our KeyCode enum.
//
//  This file compiles on Linux only. Wayland support is in WaylandWindow.cpp.
// ─────────────────────────────────────────────────────────────────────────────

#include "dexilate/platform/IWindow.h"
#include "dexilate/platform/InputEvent.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>    // XkbKeycodeToKeysym
#include <X11/Xresource.h> // XrmDatabase, XrmGetResource
// X11/Xlib.h defines None as 0L — conflicts with Modifiers::None enum.
#ifdef None
#  undef None
#endif

#include <atomic>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

namespace dexilate::platform {

// ─────────────────────────────────────────────────────────────────────────────
//  KeySym → KeyCode
// ─────────────────────────────────────────────────────────────────────────────
static KeyCode ksToKeyCode(KeySym ks) {
    using KC = KeyCode;
    switch (ks) {
        // Letters
        case XK_a: case XK_A: return KC::A;
        case XK_b: case XK_B: return KC::B;
        case XK_c: case XK_C: return KC::C;
        case XK_d: case XK_D: return KC::D;
        case XK_e: case XK_E: return KC::E;
        case XK_f: case XK_F: return KC::F;
        case XK_g: case XK_G: return KC::G;
        case XK_h: case XK_H: return KC::H;
        case XK_i: case XK_I: return KC::I;
        case XK_j: case XK_J: return KC::J;
        case XK_k: case XK_K: return KC::K;
        case XK_l: case XK_L: return KC::L;
        case XK_m: case XK_M: return KC::M;
        case XK_n: case XK_N: return KC::N;
        case XK_o: case XK_O: return KC::O;
        case XK_p: case XK_P: return KC::P;
        case XK_q: case XK_Q: return KC::Q;
        case XK_r: case XK_R: return KC::R;
        case XK_s: case XK_S: return KC::S;
        case XK_t: case XK_T: return KC::T;
        case XK_u: case XK_U: return KC::U;
        case XK_v: case XK_V: return KC::V;
        case XK_w: case XK_W: return KC::W;
        case XK_x: case XK_X: return KC::X;
        case XK_y: case XK_Y: return KC::Y;
        case XK_z: case XK_Z: return KC::Z;
        // Digits
        case XK_0: return KC::Digit0; case XK_1: return KC::Digit1;
        case XK_2: return KC::Digit2; case XK_3: return KC::Digit3;
        case XK_4: return KC::Digit4; case XK_5: return KC::Digit5;
        case XK_6: return KC::Digit6; case XK_7: return KC::Digit7;
        case XK_8: return KC::Digit8; case XK_9: return KC::Digit9;
        // Function keys
        case XK_F1:  return KC::F1;  case XK_F2:  return KC::F2;
        case XK_F3:  return KC::F3;  case XK_F4:  return KC::F4;
        case XK_F5:  return KC::F5;  case XK_F6:  return KC::F6;
        case XK_F7:  return KC::F7;  case XK_F8:  return KC::F8;
        case XK_F9:  return KC::F9;  case XK_F10: return KC::F10;
        case XK_F11: return KC::F11; case XK_F12: return KC::F12;
        case XK_F13: return KC::F13; case XK_F14: return KC::F14;
        case XK_F15: return KC::F15; case XK_F16: return KC::F16;
        case XK_F17: return KC::F17; case XK_F18: return KC::F18;
        case XK_F19: return KC::F19; case XK_F20: return KC::F20;
        // Navigation
        case XK_Escape:    return KC::Escape;
        case XK_Return:    return KC::Enter;
        case XK_Tab:       return KC::Tab;
        case XK_BackSpace: return KC::Backspace;
        case XK_Delete:    return KC::Delete;
        case XK_Insert:    return KC::Insert;
        case XK_Home:      return KC::Home;
        case XK_End:       return KC::End;
        case XK_Page_Up:   return KC::PageUp;
        case XK_Page_Down: return KC::PageDown;
        case XK_Left:      return KC::ArrowLeft;
        case XK_Right:     return KC::ArrowRight;
        case XK_Up:        return KC::ArrowUp;
        case XK_Down:      return KC::ArrowDown;
        // System
        case XK_Print:        return KC::PrintScreen;
        case XK_Scroll_Lock:  return KC::ScrollLock;
        case XK_Pause:        return KC::Pause;
        case XK_Menu:         return KC::Menu;
        // Modifiers
        case XK_Shift_L:    return KC::ShiftLeft;
        case XK_Shift_R:    return KC::ShiftRight;
        case XK_Control_L:  return KC::CtrlLeft;
        case XK_Control_R:  return KC::CtrlRight;
        case XK_Alt_L:      return KC::AltLeft;
        case XK_Alt_R:      return KC::AltRight;
        case XK_Super_L:    return KC::MetaLeft;
        case XK_Super_R:    return KC::MetaRight;
        // Numpad
        case XK_KP_0: return KC::Numpad0; case XK_KP_1: return KC::Numpad1;
        case XK_KP_2: return KC::Numpad2; case XK_KP_3: return KC::Numpad3;
        case XK_KP_4: return KC::Numpad4; case XK_KP_5: return KC::Numpad5;
        case XK_KP_6: return KC::Numpad6; case XK_KP_7: return KC::Numpad7;
        case XK_KP_8: return KC::Numpad8; case XK_KP_9: return KC::Numpad9;
        case XK_KP_Decimal: return KC::NumpadDecimal;
        case XK_KP_Enter:   return KC::NumpadEnter;
        case XK_KP_Add:     return KC::NumpadAdd;
        case XK_KP_Subtract:return KC::NumpadSubtract;
        case XK_KP_Multiply:return KC::NumpadMultiply;
        case XK_KP_Divide:  return KC::NumpadDivide;
        case XK_Num_Lock:   return KC::NumpadLock;
        // Symbols
        case XK_space:     return KC::Space;
        case XK_minus:     return KC::Minus;
        case XK_equal:     return KC::Equal;
        case XK_bracketleft:  return KC::BracketLeft;
        case XK_bracketright: return KC::BracketRight;
        case XK_backslash: return KC::Backslash;
        case XK_semicolon: return KC::Semicolon;
        case XK_apostrophe:return KC::Quote;
        case XK_comma:     return KC::Comma;
        case XK_period:    return KC::Period;
        case XK_slash:     return KC::Slash;
        case XK_grave:     return KC::Backtick;
        default:           return KC::Unknown;
    }
}

static Modifiers xModifiers(unsigned int state) {
    Modifiers m = Modifiers::None;
    if (state & ShiftMask)   m = m | Modifiers::Shift;
    if (state & ControlMask) m = m | Modifiers::Ctrl;
    if (state & Mod1Mask)    m = m | Modifiers::Alt;    // Alt / Meta
    if (state & Mod4Mask)    m = m | Modifiers::Meta;   // Super (Win key)
    if (state & LockMask)    m = m | Modifiers::CapsLock;
    return m;
}

static MouseButton xButton(unsigned int btn) {
    switch (btn) {
        case 1: return MouseButton::Left;
        case 2: return MouseButton::Middle;
        case 3: return MouseButton::Right;
        case 8: return MouseButton::X1;
        case 9: return MouseButton::X2;
        default: return MouseButton::Left;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  X11Window
// ─────────────────────────────────────────────────────────────────────────────
class X11Window final : public IWindow {
public:
    explicit X11Window(Display* dpy, const WindowDesc& desc)
        : _dpy(dpy)
    {
        _screen = DefaultScreen(_dpy);
        int depth = DefaultDepth(_dpy, _screen);
        Visual* vis = DefaultVisual(_dpy, _screen);

        // desc.width/height are LOGICAL (CSS) pixels per IWindow contract.
        // Compute physical pixels = logical × DPR for the actual X11 window.
        _dpr      = detectDpr();
        _logicalW = desc.width;
        _logicalH = desc.height;
        _physW    = static_cast<uint32_t>(std::round(static_cast<float>(desc.width)  * _dpr));
        _physH    = static_cast<uint32_t>(std::round(static_cast<float>(desc.height) * _dpr));

        XSetWindowAttributes attr = {};
        attr.background_pixel = BlackPixel(_dpy, _screen);
        attr.event_mask =
            ExposureMask         |
            KeyPressMask         | KeyReleaseMask        |
            ButtonPressMask      | ButtonReleaseMask      |
            PointerMotionMask    | LeaveWindowMask        |
            StructureNotifyMask  | FocusChangeMask;

        _win = XCreateWindow(
            _dpy, RootWindow(_dpy, _screen),
            0, 0, _physW, _physH,
            0, depth, InputOutput, vis,
            CWBackPixel | CWEventMask, &attr
        );

        if (!_win)
            throw std::runtime_error("X11Window: XCreateWindow failed");

        // Store pointer for fast lookup (property-based for cross-thread access)
        XStoreName(_dpy, _win, desc.title.c_str());

        // Intercept WM_DELETE_WINDOW so we control close behaviour
        _wmDeleteWindow = XInternAtom(_dpy, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(_dpy, _win, &_wmDeleteWindow, 1);

        // Size hints (for non-resizable windows)
        if (!desc.resizable) {
            XSizeHints hints = {};
            hints.flags      = PMinSize | PMaxSize;
            hints.min_width  = hints.max_width  = static_cast<int>(_physW);
            hints.min_height = hints.max_height = static_cast<int>(_physH);
            XSetWMNormalHints(_dpy, _win, &hints);
        }

        _winAtomic.store(reinterpret_cast<void*>(_win), std::memory_order_release);
        _dpyAtomic.store(_dpy, std::memory_order_release);

        if (desc.fullscreen) setFullscreen(true);
    }

    ~X11Window() override {
        if (_win) {
            _winAtomic.store(nullptr, std::memory_order_release);
            XDestroyWindow(_dpy, _win);
            _win = 0;
        }
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    void show() override {
        XMapRaised(_dpy, _win);
        XFlush(_dpy);
    }

    void hide() override {
        XUnmapWindow(_dpy, _win);
        XFlush(_dpy);
    }

    void close() override {
        // Synthesise a WM_DELETE_WINDOW ClientMessage (same as the user clicking ✕)
        XEvent ev = {};
        ev.xclient.type         = ClientMessage;
        ev.xclient.window       = _win;
        ev.xclient.message_type = XInternAtom(_dpy, "WM_PROTOCOLS", False);
        ev.xclient.format       = 32;
        ev.xclient.data.l[0]    = static_cast<long>(_wmDeleteWindow);
        XSendEvent(_dpy, _win, False, NoEventMask, &ev);
        XFlush(_dpy);
    }

    void pollEvents() override {
        while (XPending(_dpy) > 0) {
            XEvent ev;
            XNextEvent(_dpy, &ev);
            handleEvent(ev);
        }
    }

    [[nodiscard]] bool shouldClose() const override { return _shouldClose; }

    // ── Dimensions ────────────────────────────────────────────────────────────
    [[nodiscard]] uint32_t width()  const override { return _logicalW; }
    [[nodiscard]] uint32_t height() const override { return _logicalH; }
    [[nodiscard]] uint32_t physicalWidth()  const override { return _physW; }
    [[nodiscard]] uint32_t physicalHeight() const override { return _physH; }
    [[nodiscard]] float    devicePixelRatio() const override { return _dpr; }

    void resize(uint32_t w, uint32_t h) override {
        XResizeWindow(_dpy, _win, w, h);
        XFlush(_dpy);
    }

    // ── Title & state ─────────────────────────────────────────────────────────
    void setTitle(std::string_view title) override {
        XStoreName(_dpy, _win, std::string(title).c_str());
    }

    [[nodiscard]] std::string title() const override {
        char* name = nullptr;
        XFetchName(_dpy, _win, &name);
        std::string result = name ? name : "";
        XFree(name);
        return result;
    }

    void setFullscreen(bool fs) override {
        if (fs == _fullscreen) return;
        _fullscreen = fs;
        // Use _NET_WM_STATE_FULLSCREEN extended window hint
        Atom wmState     = XInternAtom(_dpy, "_NET_WM_STATE", False);
        Atom wmFullscreen= XInternAtom(_dpy, "_NET_WM_STATE_FULLSCREEN", False);
        XEvent ev = {};
        ev.xclient.type         = ClientMessage;
        ev.xclient.window       = _win;
        ev.xclient.message_type = wmState;
        ev.xclient.format       = 32;
        ev.xclient.data.l[0]    = fs ? 1 : 0;  // _NET_WM_STATE_ADD / _REMOVE
        ev.xclient.data.l[1]    = static_cast<long>(wmFullscreen);
        ev.xclient.data.l[3]    = 1;  // source indication = application
        XSendEvent(_dpy, DefaultRootWindow(_dpy), False,
            SubstructureNotifyMask | SubstructureRedirectMask, &ev);
        XFlush(_dpy);
    }

    [[nodiscard]] bool isFullscreen() const override { return _fullscreen; }

    // ── GPU surface ───────────────────────────────────────────────────────────
    [[nodiscard]] GpuSurfaceHandle gpuSurfaceHandle() const override {
        return {
            _winAtomic.load(std::memory_order_acquire),   // Window (XID)
            _dpyAtomic.load(std::memory_order_acquire)    // Display*
        };
    }

    void setPresentMode(PresentMode mode) override { _presentMode = mode; }
    [[nodiscard]] PresentMode presentMode() const override { return _presentMode; }
    void present() override { }  // wgpu-native drives Vulkan swap chain directly

    // ── Callbacks ─────────────────────────────────────────────────────────────
    void setEventCallback(EventCallback cb)   override { _eventCb  = std::move(cb); }
    void setResizeCallback(ResizeCallback cb) override { _resizeCb = std::move(cb); }
    void setCloseCallback(CloseCallback cb)   override { _closeCb  = std::move(cb); }

private:
    // ── DPI detection ─────────────────────────────────────────────────────────
    float detectDpr() const {
        // Try Xft.dpi resource (set by most desktop environments)
        char* rmStr = XResourceManagerString(_dpy);
        if (rmStr) {
            XrmDatabase db = XrmGetStringDatabase(rmStr);
            XrmValue val   = {};
            char* type     = nullptr;
            if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &val)
                    && val.addr) {
                float dpi = std::atof(val.addr);
                XrmDestroyDatabase(db);
                if (dpi > 0.0f) return dpi / 96.0f;
            }
            XrmDestroyDatabase(db);
        }
        // Fallback: physical screen DPI
        int mmW = DisplayWidthMM(_dpy, _screen);
        int pxW = DisplayWidth(_dpy, _screen);
        if (mmW > 0) {
            float dpi = static_cast<float>(pxW) / (static_cast<float>(mmW) / 25.4f);
            return dpi / 96.0f;
        }
        return 1.0f;
    }

    // ── X event dispatch ──────────────────────────────────────────────────────
    void handleEvent(const XEvent& ev) {
        switch (ev.type) {

        case ClientMessage:
            if (static_cast<Atom>(ev.xclient.data.l[0]) == _wmDeleteWindow) {
                _shouldClose = true;
                if (_closeCb) _closeCb();
            }
            break;

        case ConfigureNotify: {
            uint32_t newW = static_cast<uint32_t>(ev.xconfigure.width);
            uint32_t newH = static_cast<uint32_t>(ev.xconfigure.height);
            if (newW != _physW || newH != _physH) {
                _physW    = newW; _physH = newH;
                _logicalW = static_cast<uint32_t>(_physW / _dpr);
                _logicalH = static_cast<uint32_t>(_physH / _dpr);
                if (_resizeCb) _resizeCb(_logicalW, _logicalH);
                if (_eventCb)  _eventCb(WindowResizeEvent{_logicalW, _logicalH, _dpr});
            }
            break;
        }

        case FocusIn:
            if (_eventCb) _eventCb(WindowFocusEvent{true});
            break;
        case FocusOut:
            if (_eventCb) _eventCb(WindowFocusEvent{false});
            break;

        // ── Mouse ──────────────────────────────────────────────────────────
        case MotionNotify: {
            float x = lx(ev.xmotion.x), y = ly(ev.xmotion.y);
            float dx = x - _lastX, dy = y - _lastY;
            _lastX = x; _lastY = y;
            if (_eventCb) _eventCb(MouseMoveEvent{
                x, y, dx, dy, xModifiers(ev.xmotion.state)});
            break;
        }
        case LeaveNotify:
            break;  // no action needed; tracking state is implicit in X11

        case ButtonPress: {
            unsigned int btn = ev.xbutton.button;
            // Buttons 4/5/6/7 are vertical/horizontal scroll wheel
            if (btn == 4 || btn == 5 || btn == 6 || btn == 7) {
                float deltaY = (btn == 4) ? 3.0f : (btn == 5) ? -3.0f : 0.0f;
                float deltaX = (btn == 6) ? -3.0f : (btn == 7) ? 3.0f : 0.0f;
                if (_eventCb) _eventCb(ScrollEvent{
                    lx(ev.xbutton.x), ly(ev.xbutton.y),
                    deltaX, deltaY, false, xModifiers(ev.xbutton.state)});
                break;
            }
            int clicks = (ev.xbutton.time - _lastClickTime < 300u) ? 2 : 1;
            _lastClickTime = ev.xbutton.time;
            if (_eventCb) _eventCb(MouseButtonEvent{
                lx(ev.xbutton.x), ly(ev.xbutton.y),
                xButton(btn), true, clicks, xModifiers(ev.xbutton.state)});
            break;
        }
        case ButtonRelease: {
            unsigned int btn = ev.xbutton.button;
            if (btn >= 4 && btn <= 7) break;  // scroll — no release event
            if (_eventCb) _eventCb(MouseButtonEvent{
                lx(ev.xbutton.x), ly(ev.xbutton.y),
                xButton(btn), false, 1, xModifiers(ev.xbutton.state)});
            break;
        }

        // ── Keyboard ───────────────────────────────────────────────────────
        case KeyPress: {
            KeySym ks = XkbKeycodeToKeysym(_dpy,
                static_cast<unsigned char>(ev.xkey.keycode), 0, 0);
            if (_eventCb) _eventCb(KeyEvent{
                ksToKeyCode(ks),
                static_cast<uint32_t>(ev.xkey.keycode),
                true, false, xModifiers(ev.xkey.state)});
            // Produce TextInputEvent for printable characters
            char buf[8] = {};
            KeySym dummy;
            int len = XLookupString(const_cast<XKeyEvent*>(&ev.xkey),
                buf, sizeof(buf) - 1, &dummy, nullptr);
            if (len > 0 && static_cast<uint8_t>(buf[0]) >= 0x20
                        && static_cast<uint8_t>(buf[0]) != 0x7F) {
                // Decode UTF-8 first byte to codepoint
                uint8_t b = static_cast<uint8_t>(buf[0]);
                char32_t cp;
                if (b < 0x80) {
                    cp = b;
                } else if (b < 0xE0 && len >= 2) {
                    cp = ((b & 0x1F) << 6) | (static_cast<uint8_t>(buf[1]) & 0x3F);
                } else if (b < 0xF0 && len >= 3) {
                    cp = ((b & 0x0F) << 12)
                       | ((static_cast<uint8_t>(buf[1]) & 0x3F) << 6)
                       | (static_cast<uint8_t>(buf[2]) & 0x3F);
                } else {
                    cp = 0;
                }
                if (cp >= 0x20 && _eventCb)
                    _eventCb(TextInputEvent{cp});
            }
            break;
        }
        case KeyRelease: {
            KeySym ks = XkbKeycodeToKeysym(_dpy,
                static_cast<unsigned char>(ev.xkey.keycode), 0, 0);
            if (_eventCb) _eventCb(KeyEvent{
                ksToKeyCode(ks),
                static_cast<uint32_t>(ev.xkey.keycode),
                false, false, xModifiers(ev.xkey.state)});
            break;
        }

        default:
            break;
        }
    }

    // Logical pixel helpers
    float lx(int px) const { return static_cast<float>(px) / _dpr; }
    float ly(int py) const { return static_cast<float>(py) / _dpr; }

    // ── Members ───────────────────────────────────────────────────────────────
    Display*           _dpy           = nullptr;
    Window             _win           = 0;
    int                _screen        = 0;
    Atom               _wmDeleteWindow = 0;
    std::atomic<void*> _winAtomic     { nullptr };
    std::atomic<void*> _dpyAtomic     { nullptr };
    float              _dpr           = 1.0f;
    uint32_t           _physW         = 0;
    uint32_t           _physH         = 0;
    uint32_t           _logicalW      = 0;
    uint32_t           _logicalH      = 0;
    bool               _shouldClose   = false;
    bool               _fullscreen    = false;
    float              _lastX         = 0.0f;
    float              _lastY         = 0.0f;
    Time               _lastClickTime = 0;
    PresentMode        _presentMode   = PresentMode::Fifo;

    EventCallback  _eventCb;
    ResizeCallback _resizeCb;
    CloseCallback  _closeCb;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Display singleton shared across all X11Windows in this process
// ─────────────────────────────────────────────────────────────────────────────
static Display* sharedDisplay() {
    static Display* dpy = nullptr;
    if (!dpy) {
        XInitThreads();
        dpy = XOpenDisplay(nullptr);
        if (!dpy)
            throw std::runtime_error(
                "X11Window: XOpenDisplay failed — is DISPLAY set?");
    }
    return dpy;
}

// ─────────────────────────────────────────────────────────────────────────────
// The factory is defined in WaylandWindow.cpp when DEXILATE_WAYLAND is enabled,
// because that file picks the right backend at runtime. When only X11 is
// compiled in (no WaylandWindow.cpp), this file provides the factory.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef DEXILATE_WAYLAND
std::unique_ptr<IWindow> IWindow::create(const WindowDesc& desc) {
    return std::make_unique<X11Window>(sharedDisplay(), desc);
}
#endif

// Make the X11Window constructor accessible to WaylandWindow.cpp
std::unique_ptr<IWindow> createX11Window(const WindowDesc& desc) {
    return std::make_unique<X11Window>(sharedDisplay(), desc);
}

} // namespace dexilate::platform
