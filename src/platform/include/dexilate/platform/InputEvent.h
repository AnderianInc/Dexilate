// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  dexilate/platform/InputEvent.h
//
//  Normalized, platform-agnostic input event types.
//
//  All input from keyboard, mouse, stylus, and touch is converted to one of
//  these structs by the platform layer before being dispatched to the engine.
//  The engine and UI layers never see OS-specific event types (NSEvent,
//  MSG, XEvent).
//
//  Coordinate system:
//    (0,0) = top-left of the canvas viewport.
//    x increases right, y increases down.
//    Units = logical pixels (device-independent). Scale by devicePixelRatio
//    only when submitting to the GPU rasterizer.
//
//  Modifier key normalization (macOS vs Windows/Linux):
//    Shift / Alt / CapsLock / NumLock map 1:1 on all platforms.
//    Ctrl  → physical Ctrl key on all platforms. NOT remapped to Cmd on macOS.
//    Meta  → Windows/Super key on Windows/Linux; Command (⌘) key on macOS.
//
//  For cross-platform shortcuts (e.g. "undo"):
//    Check (Meta on macOS) OR (Ctrl on Windows/Linux) at the app layer.
//    Do NOT assume Ctrl == Cmd. They are distinct physical keys on macOS.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <variant>

namespace dexilate::platform {

// ── Modifier keys ─────────────────────────────────────────────────────────────
enum class Modifiers : uint32_t {
    None     = 0,
    Shift    = 1 << 0,
    Ctrl     = 1 << 1,   ///< Physical Ctrl key on all platforms
    Alt      = 1 << 2,   ///< Alt on Windows/Linux; Option (⌥) on macOS
    Meta     = 1 << 3,   ///< Windows/Super key on Win/Linux; Command (⌘) on macOS
    CapsLock = 1 << 4,
    NumLock  = 1 << 5,
};
inline Modifiers operator|(Modifiers a, Modifiers b) {
    return static_cast<Modifiers>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool hasModifier(Modifiers mods, Modifiers flag) {
    return (static_cast<uint32_t>(mods) & static_cast<uint32_t>(flag)) != 0;
}

// ── Mouse buttons ─────────────────────────────────────────────────────────────
enum class MouseButton : uint8_t {
    Left   = 0,
    Right  = 1,
    Middle = 2,
    X1     = 3,   ///< Side button (back)
    X2     = 4,   ///< Side button (forward)
};

// ── Key codes (logical, layout-independent) ───────────────────────────────────
// Values for A–Z and 0–9 match ASCII for easy lookup tables.
// Values >= 256 are Dexilate-defined with no ASCII equivalent.
enum class KeyCode : uint32_t {
    Unknown = 0,

    // Letters (ASCII values)
    A=65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Digits — top row (ASCII values)
    Digit0=48, Digit1, Digit2, Digit3, Digit4,
    Digit5,    Digit6, Digit7, Digit8, Digit9,

    // Function keys
    F1=112, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13, F14, F15, F16, F17, F18, F19, F20,

    // Navigation / editing
    Escape=256, Enter, Tab, Backspace, Delete, Insert,
    Home, End, PageUp, PageDown,
    ArrowLeft, ArrowRight, ArrowUp, ArrowDown,

    // System keys
    PrintScreen, ScrollLock, Pause,

    // Modifier keys (physical key positions, both sides)
    ShiftLeft, ShiftRight,
    CtrlLeft,  CtrlRight,
    AltLeft,   AltRight,
    MetaLeft,  MetaRight,  ///< Windows/Super (Win/Linux) or Command (macOS)
    Menu,                   ///< Application / context-menu key

    // Numpad
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4,
    Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadDecimal, NumpadEnter, NumpadAdd, NumpadSubtract,
    NumpadMultiply, NumpadDivide, NumpadLock,

    // Common symbols (ASCII values where applicable)
    Space=32, Minus=45, Equal=61,
    BracketLeft=91, BracketRight=93,
    Backslash=92, Semicolon=59, Quote=39,
    Comma=44, Period=46, Slash=47, Backtick=96,
};

// ── Mouse/pointer events ──────────────────────────────────────────────────────
struct MouseMoveEvent {
    float x, y;           ///< Logical pixel position
    float deltaX, deltaY; ///< Delta from last position
    Modifiers modifiers = Modifiers::None;
};

struct MouseButtonEvent {
    float       x, y;
    MouseButton button;
    bool        pressed;        ///< true = down, false = up
    int         clickCount = 1; ///< 2 for double-click
    Modifiers   modifiers = Modifiers::None;
};

struct ScrollEvent {
    float x, y;                ///< Cursor position
    float deltaX, deltaY;      ///< Scroll deltas in logical pixels
    bool  isTrackpad = false;  ///< Smooth trackpad scroll vs. detented click wheel
    Modifiers modifiers = Modifiers::None;
};

// ── Keyboard events ───────────────────────────────────────────────────────────
struct KeyEvent {
    KeyCode   key;
    uint32_t  scancode;        ///< Physical key position (platform-specific)
    bool      pressed;         ///< true = down, false = up
    bool      repeat = false;  ///< Key held, OS auto-repeat firing
    Modifiers modifiers = Modifiers::None;
};

struct TextInputEvent {
    char32_t codepoint;   ///< Unicode codepoint for composed text entry
};

// ── Stylus / tablet events ────────────────────────────────────────────────────
struct StylusEvent {
    float x, y;                 ///< Logical pixel position
    float pressure;             ///< [0.0, 1.0] — 0 = hovering, 1 = full pressure
    float tiltX;                ///< [-90.0, +90.0] degrees from vertical, X axis
    float tiltY;                ///< [-90.0, +90.0] degrees from vertical, Y axis
    float rotation;             ///< [0.0, 360.0] degrees barrel rotation
    bool  eraser      = false;  ///< true when eraser end is active
    bool  inProximity = false;  ///< true when stylus is near but not touching
    Modifiers modifiers = Modifiers::None;
};

// ── Touch events ──────────────────────────────────────────────────────────────
struct TouchPoint {
    uint32_t id;      ///< Unique identifier for this touch contact (per gesture)
    float    x, y;   ///< Logical pixel position
    float    pressure;///< [0.0, 1.0]; 1.0 if device does not report pressure
};

struct TouchEvent {
    // 10 simultaneous contacts covers all current multi-touch hardware.
    // Stylus input (Apple Pencil, Wacom) is delivered as StylusEvent, not TouchEvent.
    static constexpr uint8_t MAX_TOUCHES = 10;
    TouchPoint points[MAX_TOUCHES];
    uint8_t    count = 0;
    enum class Phase : uint8_t { Began, Moved, Ended, Cancelled } phase;
};

// ── Window lifecycle ──────────────────────────────────────────────────────────
struct WindowResizeEvent {
    uint32_t width, height;   ///< New logical dimensions
    float    devicePixelRatio;
};

struct WindowFocusEvent {
    bool gained;  ///< true = focus gained, false = focus lost
};

// ── Unified event variant ─────────────────────────────────────────────────────
using InputEvent = std::variant<
    MouseMoveEvent,
    MouseButtonEvent,
    ScrollEvent,
    KeyEvent,
    TextInputEvent,
    StylusEvent,
    TouchEvent,
    WindowResizeEvent,
    WindowFocusEvent
>;

} // namespace dexilate::platform
