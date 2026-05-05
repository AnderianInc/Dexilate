// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/platform/src/windows/Win32Window.cpp
//  IWindow implementation for Windows — HWND + DXGI surface
//
//  Architecture
//  ────────────
//  Win32Window    C++ class implementing IWindow. Owns the HWND and translates
//                 WM_* messages into typed InputEvents.
//
//  IWindow::create()  Factory — sets per-monitor-v2 DPI awareness on first
//                     call, registers WNDCLASSEX, creates HWND.
//
//  Coordinate system
//  ──────────────────
//  Win32 client coordinates are top-left origin (same as Dexilate) so no Y-flip
//  is needed. Raw coordinates are in physical pixels; we divide by
//  devicePixelRatio() to produce the logical-pixel values Dexilate expects.
//
//  Thread safety
//  ─────────────
//  All Win32 GUI calls must run on the thread that owns the HWND (main thread).
//  gpuSurfaceHandle() and present() are the only GPU-thread-safe methods.
// ─────────────────────────────────────────────────────────────────────────────

#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0A00   // Windows 10+ for DPI APIs
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "dexilate/platform/IWindow.h"
#include "dexilate/platform/InputEvent.h"

#include <atomic>
#include <stdexcept>
#include <string>

namespace dexilate::platform {

// ─────────────────────────────────────────────────────────────────────────────
//  VK_ → KeyCode
// ─────────────────────────────────────────────────────────────────────────────
static KeyCode vkToKeyCode(WPARAM vk, LPARAM lParam) {
    using KC = KeyCode;
    const bool ext = (lParam & (1 << 24)) != 0;
    switch (vk) {
        // Letters
        case 'A': return KC::A;  case 'B': return KC::B;  case 'C': return KC::C;
        case 'D': return KC::D;  case 'E': return KC::E;  case 'F': return KC::F;
        case 'G': return KC::G;  case 'H': return KC::H;  case 'I': return KC::I;
        case 'J': return KC::J;  case 'K': return KC::K;  case 'L': return KC::L;
        case 'M': return KC::M;  case 'N': return KC::N;  case 'O': return KC::O;
        case 'P': return KC::P;  case 'Q': return KC::Q;  case 'R': return KC::R;
        case 'S': return KC::S;  case 'T': return KC::T;  case 'U': return KC::U;
        case 'V': return KC::V;  case 'W': return KC::W;  case 'X': return KC::X;
        case 'Y': return KC::Y;  case 'Z': return KC::Z;
        // Digits
        case '0': return KC::Digit0; case '1': return KC::Digit1;
        case '2': return KC::Digit2; case '3': return KC::Digit3;
        case '4': return KC::Digit4; case '5': return KC::Digit5;
        case '6': return KC::Digit6; case '7': return KC::Digit7;
        case '8': return KC::Digit8; case '9': return KC::Digit9;
        // Function keys
        case VK_F1:  return KC::F1;  case VK_F2:  return KC::F2;
        case VK_F3:  return KC::F3;  case VK_F4:  return KC::F4;
        case VK_F5:  return KC::F5;  case VK_F6:  return KC::F6;
        case VK_F7:  return KC::F7;  case VK_F8:  return KC::F8;
        case VK_F9:  return KC::F9;  case VK_F10: return KC::F10;
        case VK_F11: return KC::F11; case VK_F12: return KC::F12;
        case VK_F13: return KC::F13; case VK_F14: return KC::F14;
        case VK_F15: return KC::F15; case VK_F16: return KC::F16;
        case VK_F17: return KC::F17; case VK_F18: return KC::F18;
        case VK_F19: return KC::F19; case VK_F20: return KC::F20;
        // Navigation
        case VK_ESCAPE: return KC::Escape;
        case VK_RETURN: return ext ? KC::NumpadEnter : KC::Enter;
        case VK_TAB:    return KC::Tab;
        case VK_BACK:   return KC::Backspace;
        case VK_DELETE: return KC::Delete;
        case VK_INSERT: return KC::Insert;
        case VK_HOME:   return KC::Home;
        case VK_END:    return KC::End;
        case VK_PRIOR:  return KC::PageUp;
        case VK_NEXT:   return KC::PageDown;
        case VK_LEFT:   return KC::ArrowLeft;
        case VK_RIGHT:  return KC::ArrowRight;
        case VK_UP:     return KC::ArrowUp;
        case VK_DOWN:   return KC::ArrowDown;
        // System
        case VK_SNAPSHOT: return KC::PrintScreen;
        case VK_SCROLL:   return KC::ScrollLock;
        case VK_PAUSE:    return KC::Pause;
        case VK_APPS:     return KC::Menu;
        // Modifiers — use extended bit to distinguish left/right
        case VK_SHIFT:   return ext ? KC::ShiftRight : KC::ShiftLeft;
        case VK_CONTROL: return ext ? KC::CtrlRight  : KC::CtrlLeft;
        case VK_MENU:    return ext ? KC::AltRight   : KC::AltLeft;
        case VK_LWIN:    return KC::MetaLeft;
        case VK_RWIN:    return KC::MetaRight;
        // Numpad
        case VK_NUMPAD0:  return KC::Numpad0;  case VK_NUMPAD1: return KC::Numpad1;
        case VK_NUMPAD2:  return KC::Numpad2;  case VK_NUMPAD3: return KC::Numpad3;
        case VK_NUMPAD4:  return KC::Numpad4;  case VK_NUMPAD5: return KC::Numpad5;
        case VK_NUMPAD6:  return KC::Numpad6;  case VK_NUMPAD7: return KC::Numpad7;
        case VK_NUMPAD8:  return KC::Numpad8;  case VK_NUMPAD9: return KC::Numpad9;
        case VK_DECIMAL:  return KC::NumpadDecimal;
        case VK_ADD:      return KC::NumpadAdd;
        case VK_SUBTRACT: return KC::NumpadSubtract;
        case VK_MULTIPLY: return KC::NumpadMultiply;
        case VK_DIVIDE:   return KC::NumpadDivide;
        case VK_NUMLOCK:  return KC::NumpadLock;
        // Symbols
        case VK_SPACE:      return KC::Space;
        case VK_OEM_MINUS:  return KC::Minus;
        case VK_OEM_PLUS:   return KC::Equal;
        case VK_OEM_4:      return KC::BracketLeft;
        case VK_OEM_6:      return KC::BracketRight;
        case VK_OEM_5:      return KC::Backslash;
        case VK_OEM_1:      return KC::Semicolon;
        case VK_OEM_7:      return KC::Quote;
        case VK_OEM_COMMA:  return KC::Comma;
        case VK_OEM_PERIOD: return KC::Period;
        case VK_OEM_2:      return KC::Slash;
        case VK_OEM_3:      return KC::Backtick;
        default:            return KC::Unknown;
    }
}

static Modifiers currentModifiers() {
    Modifiers m = Modifiers::None;
    if (GetKeyState(VK_SHIFT)   & 0x8000) m = m | Modifiers::Shift;
    if (GetKeyState(VK_CONTROL) & 0x8000) m = m | Modifiers::Ctrl;
    if (GetKeyState(VK_MENU)    & 0x8000) m = m | Modifiers::Alt;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000)
        m = m | Modifiers::Meta;
    if (GetKeyState(VK_CAPITAL) & 0x0001) m = m | Modifiers::CapsLock;
    if (GetKeyState(VK_NUMLOCK) & 0x0001) m = m | Modifiers::NumLock;
    return m;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Win32Window
// ─────────────────────────────────────────────────────────────────────────────
class Win32Window final : public IWindow {
public:
    explicit Win32Window(const WindowDesc& desc) {
        ensureClassRegistered();

        DWORD style = WS_OVERLAPPEDWINDOW;
        if (!desc.resizable)
            style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

        // Compute the window frame size from the desired client area
        RECT r = { 0, 0,
            static_cast<LONG>(desc.width),
            static_cast<LONG>(desc.height) };
        AdjustWindowRect(&r, style, FALSE);

        std::wstring wt(desc.title.begin(), desc.title.end());
        _hwnd = CreateWindowExW(
            0, k_wndClass, wt.c_str(), style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            r.right - r.left, r.bottom - r.top,
            nullptr, nullptr, GetModuleHandleW(nullptr), this
        );
        if (!_hwnd)
            throw std::runtime_error("Win32Window: CreateWindowEx failed");

        _dpi = GetDpiForWindow(_hwnd);
        updateClientSize();
        _hwndAtomic.store(_hwnd, std::memory_order_release);

        if (desc.maximized)  ShowWindow(_hwnd, SW_MAXIMIZE);
        if (desc.fullscreen) setFullscreen(true);
    }

    ~Win32Window() override {
        if (_hwnd) {
            _hwndAtomic.store(nullptr, std::memory_order_release);
            DestroyWindow(_hwnd);
            _hwnd = nullptr;
        }
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    void show()  override { ShowWindow(_hwnd, SW_SHOW); UpdateWindow(_hwnd); }
    void hide()  override { ShowWindow(_hwnd, SW_HIDE); }
    void close() override { PostMessageW(_hwnd, WM_CLOSE, 0, 0); }

    void pollEvents() override {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    [[nodiscard]] bool shouldClose() const override { return _shouldClose; }

    // ── Dimensions ────────────────────────────────────────────────────────────
    [[nodiscard]] uint32_t width()  const override { return _logicalW; }
    [[nodiscard]] uint32_t height() const override { return _logicalH; }
    [[nodiscard]] uint32_t physicalWidth()  const override { return _physW; }
    [[nodiscard]] uint32_t physicalHeight() const override { return _physH; }
    [[nodiscard]] float    devicePixelRatio() const override {
        return static_cast<float>(_dpi) / 96.0f;
    }

    void resize(uint32_t w, uint32_t h) override {
        DWORD style = static_cast<DWORD>(GetWindowLongW(_hwnd, GWL_STYLE));
        RECT r = { 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };
        AdjustWindowRect(&r, style, FALSE);
        SetWindowPos(_hwnd, nullptr, 0, 0,
            r.right - r.left, r.bottom - r.top,
            SWP_NOMOVE | SWP_NOZORDER);
    }

    // ── Title & state ─────────────────────────────────────────────────────────
    void setTitle(std::string_view title) override {
        std::wstring wt(title.begin(), title.end());
        SetWindowTextW(_hwnd, wt.c_str());
    }

    [[nodiscard]] std::string title() const override {
        wchar_t buf[512] = {};
        GetWindowTextW(_hwnd, buf, 512);
        std::wstring wt = buf;
        return {wt.begin(), wt.end()};
    }

    void setFullscreen(bool fs) override {
        if (fs == _fullscreen) return;
        _fullscreen = fs;
        if (fs) {
            GetWindowPlacement(_hwnd, &_savedPlacement);
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfoW(
                MonitorFromWindow(_hwnd, MONITOR_DEFAULTTONEAREST), &mi);
            SetWindowLongW(_hwnd, GWL_STYLE,
                GetWindowLongW(_hwnd, GWL_STYLE) & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(_hwnd, HWND_TOP,
                mi.rcMonitor.left, mi.rcMonitor.top,
                mi.rcMonitor.right  - mi.rcMonitor.left,
                mi.rcMonitor.bottom - mi.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        } else {
            SetWindowLongW(_hwnd, GWL_STYLE,
                GetWindowLongW(_hwnd, GWL_STYLE) | WS_OVERLAPPEDWINDOW);
            SetWindowPlacement(_hwnd, &_savedPlacement);
            SetWindowPos(_hwnd, nullptr, 0, 0, 0, 0,
                SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER);
        }
    }

    [[nodiscard]] bool isFullscreen() const override { return _fullscreen; }

    // ── GPU surface ───────────────────────────────────────────────────────────
    [[nodiscard]] GpuSurfaceHandle gpuSurfaceHandle() const override {
        return { _hwndAtomic.load(std::memory_order_acquire), nullptr };
    }

    void setPresentMode(PresentMode mode) override { _presentMode = mode; }
    [[nodiscard]] PresentMode presentMode() const override { return _presentMode; }

    void present() override { }  // wgpu-native drives DXGI swap chain directly

    // ── Callbacks ─────────────────────────────────────────────────────────────
    void setEventCallback(EventCallback cb)   override { _eventCb  = std::move(cb); }
    void setResizeCallback(ResizeCallback cb) override { _resizeCb = std::move(cb); }
    void setCloseCallback(CloseCallback cb)   override { _closeCb  = std::move(cb); }

private:
    // ── Class registration ────────────────────────────────────────────────────
    static void ensureClassRegistered() {
        static bool done = false;
        if (done) return;
        WNDCLASSEXW wc    = {};
        wc.cbSize         = sizeof(wc);
        wc.style          = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc    = &Win32Window::WndProc;
        wc.hInstance      = GetModuleHandleW(nullptr);
        wc.hCursor        = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName  = k_wndClass;
        if (!RegisterClassExW(&wc))
            throw std::runtime_error("Win32Window: RegisterClassEx failed");
        done = true;
    }

    // ── WndProc ───────────────────────────────────────────────────────────────
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        Win32Window* self = nullptr;
        if (msg == WM_NCCREATE) {
            // Store 'this' before any other message arrives
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = static_cast<Win32Window*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<Win32Window*>(
                GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        if (self) return self->handleMessage(hwnd, msg, wp, lp);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {

        case WM_CLOSE:
            _shouldClose = true;
            if (_closeCb) _closeCb();
            return 0;

        case WM_SIZE: {
            updateClientSize();
            float dpr = devicePixelRatio();
            if (_resizeCb) _resizeCb(_logicalW, _logicalH);
            if (_eventCb)  _eventCb(WindowResizeEvent{_logicalW, _logicalH, dpr});
            return 0;
        }

        case WM_DPICHANGED: {
            _dpi = HIWORD(wp);
            const auto* r = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(hwnd, nullptr,
                r->left, r->top, r->right - r->left, r->bottom - r->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }

        case WM_SETFOCUS:
            if (_eventCb) _eventCb(WindowFocusEvent{true});
            return 0;
        case WM_KILLFOCUS:
            if (_eventCb) _eventCb(WindowFocusEvent{false});
            return 0;

        // ── Mouse ──────────────────────────────────────────────────────────
        case WM_MOUSEMOVE: {
            if (!_mouseTracking) {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
                _mouseTracking = true;
            }
            float x = lx(lp), y = ly(lp);
            float dx = x - _lastX, dy = y - _lastY;
            _lastX = x; _lastY = y;
            if (_eventCb) _eventCb(MouseMoveEvent{x, y, dx, dy, currentModifiers()});
            return 0;
        }
        case WM_MOUSELEAVE:
            _mouseTracking = false;
            return 0;

        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
            SetCapture(hwnd);
            dispatchButton(lp, MouseButton::Left, true,
                msg == WM_LBUTTONDBLCLK ? 2 : 1);
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            dispatchButton(lp, MouseButton::Left, false, 1);
            return 0;
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
            dispatchButton(lp, MouseButton::Right, true,
                msg == WM_RBUTTONDBLCLK ? 2 : 1);
            return 0;
        case WM_RBUTTONUP:
            dispatchButton(lp, MouseButton::Right, false, 1);
            return 0;
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
            dispatchButton(lp, MouseButton::Middle, true,
                msg == WM_MBUTTONDBLCLK ? 2 : 1);
            return 0;
        case WM_MBUTTONUP:
            dispatchButton(lp, MouseButton::Middle, false, 1);
            return 0;
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
            dispatchButton(lp,
                GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? MouseButton::X1 : MouseButton::X2,
                true, msg == WM_XBUTTONDBLCLK ? 2 : 1);
            return TRUE;
        case WM_XBUTTONUP:
            dispatchButton(lp,
                GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? MouseButton::X1 : MouseButton::X2,
                false, 1);
            return TRUE;

        case WM_MOUSEWHEEL: {
            // lp holds screen coordinates — convert to client
            POINT pt = { static_cast<SHORT>(LOWORD(lp)),
                         static_cast<SHORT>(HIWORD(lp)) };
            ScreenToClient(hwnd, &pt);
            float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp))
                        / static_cast<float>(WHEEL_DELTA);
            if (_eventCb) _eventCb(ScrollEvent{
                lxRaw(pt.x), lyRaw(pt.y),
                0.0f, delta * 3.0f, false, currentModifiers()});
            return 0;
        }
        case WM_MOUSEHWHEEL: {
            POINT pt = { static_cast<SHORT>(LOWORD(lp)),
                         static_cast<SHORT>(HIWORD(lp)) };
            ScreenToClient(hwnd, &pt);
            float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp))
                        / static_cast<float>(WHEEL_DELTA);
            if (_eventCb) _eventCb(ScrollEvent{
                lxRaw(pt.x), lyRaw(pt.y),
                delta * 3.0f, 0.0f, false, currentModifiers()});
            return 0;
        }

        // ── Keyboard ───────────────────────────────────────────────────────
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (_eventCb) _eventCb(KeyEvent{
                vkToKeyCode(wp, lp),
                static_cast<uint32_t>((lp >> 16) & 0xFF),
                true,
                (lp & (1 << 30)) != 0,  // repeat flag
                currentModifiers()});
            return 0;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (_eventCb) _eventCb(KeyEvent{
                vkToKeyCode(wp, lp),
                static_cast<uint32_t>((lp >> 16) & 0xFF),
                false, false, currentModifiers()});
            return 0;

        case WM_CHAR:
        case WM_UNICHAR:
            if (wp == UNICODE_NOCHAR) return TRUE;  // WM_UNICHAR capability probe
            if (wp >= 0x20 && wp != 0x7F) {
                // Handle UTF-16 surrogate pairs (supplementary plane chars)
                if (wp >= 0xD800 && wp <= 0xDBFF) {
                    _surrogateHigh = static_cast<uint32_t>(wp);
                    return 0;
                }
                uint32_t cp;
                if (wp >= 0xDC00 && wp <= 0xDFFF && _surrogateHigh) {
                    cp = 0x10000u + ((_surrogateHigh - 0xD800u) << 10u)
                                  + (static_cast<uint32_t>(wp) - 0xDC00u);
                    _surrogateHigh = 0;
                } else {
                    _surrogateHigh = 0;
                    cp = static_cast<uint32_t>(wp);
                }
                if (_eventCb) _eventCb(TextInputEvent{static_cast<char32_t>(cp)});
            }
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
    }

    // ── Helpers ───────────────────────────────────────────────────────────────
    void updateClientSize() {
        RECT cr = {};
        GetClientRect(_hwnd, &cr);
        _physW    = static_cast<uint32_t>(cr.right);
        _physH    = static_cast<uint32_t>(cr.bottom);
        float dpr = devicePixelRatio();
        _logicalW = static_cast<uint32_t>(_physW / dpr);
        _logicalH = static_cast<uint32_t>(_physH / dpr);
    }

    // Convert signed LPARAM client coordinates to logical pixels
    float lx(LPARAM lp) const {
        return static_cast<float>(static_cast<SHORT>(LOWORD(lp))) / devicePixelRatio();
    }
    float ly(LPARAM lp) const {
        return static_cast<float>(static_cast<SHORT>(HIWORD(lp))) / devicePixelRatio();
    }
    float lxRaw(LONG x) const { return static_cast<float>(x) / devicePixelRatio(); }
    float lyRaw(LONG y) const { return static_cast<float>(y) / devicePixelRatio(); }

    void dispatchButton(LPARAM lp, MouseButton btn, bool pressed, int clicks) {
        if (_eventCb) _eventCb(MouseButtonEvent{
            lx(lp), ly(lp), btn, pressed, clicks, currentModifiers()});
    }

    // ── Members ───────────────────────────────────────────────────────────────
    HWND               _hwnd           = nullptr;
    std::atomic<void*> _hwndAtomic     { nullptr };
    UINT               _dpi            = 96;
    uint32_t           _logicalW       = 0;
    uint32_t           _logicalH       = 0;
    uint32_t           _physW          = 0;
    uint32_t           _physH          = 0;
    bool               _shouldClose    = false;
    bool               _fullscreen     = false;
    bool               _mouseTracking  = false;
    float              _lastX          = 0.0f;
    float              _lastY          = 0.0f;
    uint32_t           _surrogateHigh  = 0;
    PresentMode        _presentMode    = PresentMode::Fifo;
    WINDOWPLACEMENT    _savedPlacement = { sizeof(WINDOWPLACEMENT) };

    EventCallback  _eventCb;
    ResizeCallback _resizeCb;
    CloseCallback  _closeCb;

    static constexpr const wchar_t* k_wndClass = L"DexilateWindow";
};

// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<IWindow> IWindow::create(const WindowDesc& desc) {
    // Per-monitor v2 DPI awareness must be set before any HWND is created.
    // Harmless if called multiple times — Windows ignores repeated calls.
    static bool s_dpiSet = false;
    if (!s_dpiSet) {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        s_dpiSet = true;
    }
    return std::make_unique<Win32Window>(desc);
}

} // namespace dexilate::platform
