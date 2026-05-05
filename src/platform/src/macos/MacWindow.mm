// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/platform/src/macos/MacWindow.mm
//
//  IWindow implementation for macOS — NSWindow + CAMetalLayer.
//
//  Architecture
//  ────────────
//  DexilateMetalView   NSView subclass that hosts a CAMetalLayer and forwards
//                    all mouse/key/scroll/stylus NSEvents to MacWindow.
//
//  DexilateWindowDelegate  NSWindowDelegate that handles resize, DPI change,
//                        focus, and close requests.
//
//  MacWindow         C++ class implementing IWindow. Owns the NSWindow,
//                    view, layer, and delegate.
//
//  IWindow::create() Factory — initialises NSApplication if needed, returns
//                    std::unique_ptr<MacWindow>.
//
//  Coordinate system
//  ──────────────────
//  macOS origin is bottom-left; Dexilate expects top-left.  All Y values from
//  NSEvent are flipped before being placed in InputEvent structs.
//
//  Thread safety
//  ─────────────
//  All methods are main-thread only except present() and gpuSurfaceHandle(),
//  which are GPU-thread safe after show() returns.
// ─────────────────────────────────────────────────────────────────────────────

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#include <atomic>

#include "dexilate/platform/IWindow.h"
#include "dexilate/platform/InputEvent.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Forward declare the C++ implementation class
// ─────────────────────────────────────────────────────────────────────────────
namespace dexilate::platform { class MacWindow; }

// ─────────────────────────────────────────────────────────────────────────────
//  DexilateMetalView — NSView hosting CAMetalLayer, first responder for input
// ─────────────────────────────────────────────────────────────────────────────
@interface DexilateMetalView : NSView
@property (nonatomic, assign) dexilate::platform::MacWindow* cppWindow;
@end

// ─────────────────────────────────────────────────────────────────────────────
//  DexilateWindowDelegate — NSWindowDelegate for lifecycle events
// ─────────────────────────────────────────────────────────────────────────────
@interface DexilateWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) dexilate::platform::MacWindow* cppWindow;
@end

// ─────────────────────────────────────────────────────────────────────────────
//  macOS virtual key code → KeyCode lookup table
//  Values from Carbon/HIToolbox/Events.h — stable across macOS versions.
// ─────────────────────────────────────────────────────────────────────────────
static dexilate::platform::KeyCode vkToKeyCode(unsigned short vk) {
    using KC = dexilate::platform::KeyCode;
    switch (vk) {
        // Letters
        case 0x00: return KC::A;  case 0x0B: return KC::B;
        case 0x08: return KC::C;  case 0x02: return KC::D;
        case 0x0E: return KC::E;  case 0x03: return KC::F;
        case 0x05: return KC::G;  case 0x04: return KC::H;
        case 0x22: return KC::I;  case 0x26: return KC::J;
        case 0x28: return KC::K;  case 0x25: return KC::L;
        case 0x2E: return KC::M;  case 0x2D: return KC::N;
        case 0x1F: return KC::O;  case 0x23: return KC::P;
        case 0x0C: return KC::Q;  case 0x0F: return KC::R;
        case 0x01: return KC::S;  case 0x11: return KC::T;
        case 0x20: return KC::U;  case 0x09: return KC::V;
        case 0x0D: return KC::W;  case 0x07: return KC::X;
        case 0x10: return KC::Y;  case 0x06: return KC::Z;
        // Digits
        case 0x1D: return KC::Digit0; case 0x12: return KC::Digit1;
        case 0x13: return KC::Digit2; case 0x14: return KC::Digit3;
        case 0x15: return KC::Digit4; case 0x17: return KC::Digit5;
        case 0x16: return KC::Digit6; case 0x1A: return KC::Digit7;
        case 0x1C: return KC::Digit8; case 0x19: return KC::Digit9;
        // Function keys
        case 0x7A: return KC::F1;  case 0x78: return KC::F2;
        case 0x63: return KC::F3;  case 0x76: return KC::F4;
        case 0x60: return KC::F5;  case 0x61: return KC::F6;
        case 0x62: return KC::F7;  case 0x64: return KC::F8;
        case 0x65: return KC::F9;  case 0x6D: return KC::F10;
        case 0x67: return KC::F11; case 0x6F: return KC::F12;
        case 0x69: return KC::F13; case 0x6B: return KC::F14;
        case 0x71: return KC::F15; case 0x6A: return KC::F16;
        case 0x40: return KC::F17; case 0x4F: return KC::F18;
        case 0x50: return KC::F19; case 0x5A: return KC::F20;
        // Navigation / control
        case 0x35: return KC::Escape;
        case 0x24: return KC::Enter;
        case 0x30: return KC::Tab;
        case 0x33: return KC::Backspace;   // kVK_Delete
        case 0x75: return KC::Delete;      // kVK_ForwardDelete
        case 0x72: return KC::Insert;      // kVK_Help
        case 0x73: return KC::Home;
        case 0x77: return KC::End;
        case 0x74: return KC::PageUp;
        case 0x79: return KC::PageDown;
        case 0x7B: return KC::ArrowLeft;
        case 0x7C: return KC::ArrowRight;
        case 0x7E: return KC::ArrowUp;
        case 0x7D: return KC::ArrowDown;
        // Symbols
        case 0x31: return KC::Space;
        case 0x1B: return KC::Minus;
        case 0x18: return KC::Equal;
        case 0x21: return KC::BracketLeft;
        case 0x1E: return KC::BracketRight;
        case 0x2A: return KC::Backslash;
        case 0x29: return KC::Semicolon;
        case 0x27: return KC::Quote;
        case 0x2B: return KC::Comma;
        case 0x2F: return KC::Period;
        case 0x2C: return KC::Slash;
        case 0x32: return KC::Backtick;
        // Modifiers (physical)
        case 0x38: return KC::ShiftLeft;  case 0x3C: return KC::ShiftRight;
        case 0x3B: return KC::CtrlLeft;   case 0x3E: return KC::CtrlRight;
        case 0x3A: return KC::AltLeft;    case 0x3D: return KC::AltRight;
        case 0x37: return KC::MetaLeft;   case 0x36: return KC::MetaRight;
        // Numpad
        case 0x52: return KC::Numpad0; case 0x53: return KC::Numpad1;
        case 0x54: return KC::Numpad2; case 0x55: return KC::Numpad3;
        case 0x56: return KC::Numpad4; case 0x57: return KC::Numpad5;
        case 0x58: return KC::Numpad6; case 0x59: return KC::Numpad7;
        case 0x5B: return KC::Numpad8; case 0x5C: return KC::Numpad9;
        case 0x41: return KC::NumpadDecimal;
        case 0x4C: return KC::NumpadEnter;
        case 0x45: return KC::NumpadAdd;
        case 0x4E: return KC::NumpadSubtract;
        case 0x43: return KC::NumpadMultiply;
        case 0x4B: return KC::NumpadDivide;
        case 0x47: return KC::NumpadLock;
        default:   return KC::Unknown;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Modifier extraction
// ─────────────────────────────────────────────────────────────────────────────
static dexilate::platform::Modifiers modifiersFromFlags(NSEventModifierFlags flags) {
    using M = dexilate::platform::Modifiers;
    uint32_t m = 0;
    if (flags & NSEventModifierFlagShift)    m |= uint32_t(M::Shift);
    if (flags & NSEventModifierFlagControl)  m |= uint32_t(M::Ctrl);
    if (flags & NSEventModifierFlagOption)   m |= uint32_t(M::Alt);
    if (flags & NSEventModifierFlagCommand)  m |= uint32_t(M::Meta);
    if (flags & NSEventModifierFlagCapsLock) m |= uint32_t(M::CapsLock);
    if (flags & NSEventModifierFlagNumericPad) m |= uint32_t(M::NumLock);
    return static_cast<M>(m);
}

// ─────────────────────────────────────────────────────────────────────────────
//  MacWindow — C++ implementation of IWindow
// ─────────────────────────────────────────────────────────────────────────────
namespace dexilate::platform {

class MacWindow final : public IWindow {
public:
    explicit MacWindow(const WindowDesc& desc);
    ~MacWindow() override;

    // IWindow
    void show()  override;
    void hide()  override;
    void close() override;
    void pollEvents() override;
    [[nodiscard]] bool shouldClose() const override;

    [[nodiscard]] uint32_t width()  const override;
    [[nodiscard]] uint32_t height() const override;
    [[nodiscard]] uint32_t physicalWidth()  const override;
    [[nodiscard]] uint32_t physicalHeight() const override;
    [[nodiscard]] float    devicePixelRatio() const override;
    void resize(uint32_t w, uint32_t h) override;

    void setTitle(std::string_view t) override;
    [[nodiscard]] std::string title() const override;
    void setFullscreen(bool fs) override;
    [[nodiscard]] bool isFullscreen() const override;

    [[nodiscard]] GpuSurfaceHandle gpuSurfaceHandle() const override;
    void setPresentMode(PresentMode mode) override;
    [[nodiscard]] PresentMode presentMode() const override;
    void present() override;

    void setEventCallback(EventCallback cb)   override { _eventCb  = std::move(cb); }
    void setResizeCallback(ResizeCallback cb) override { _resizeCb = std::move(cb); }
    void setCloseCallback(CloseCallback cb)   override { _closeCb  = std::move(cb); }

    // Called by ObjC helpers
    void onResize(uint32_t w, uint32_t h);
    void onDpiChange(float dpr);
    void onFocus(bool gained);
    void onCloseRequested();
    void dispatchEvent(const InputEvent& e) { if (_eventCb) _eventCb(e); }

    // NSEvent → InputEvent converters (called from DexilateMetalView)
    void handleMouseMove(NSEvent* event);
    void handleMouseButton(NSEvent* event, bool pressed, MouseButton btn);
    void handleScroll(NSEvent* event);
    void handleKeyDown(NSEvent* event);
    void handleKeyUp(NSEvent* event);
    void handleFlagsChanged(NSEvent* event);
    void handleTabletPoint(NSEvent* event);

private:
    void applyPresentModeToLayer();
    [[nodiscard]] float viewHeight() const;
    [[nodiscard]] std::pair<float,float> flipY(CGPoint p) const;

    NSWindow* __strong             _window;
    DexilateMetalView* __strong      _view;
    CAMetalLayer* __strong         _metalLayer;
    DexilateWindowDelegate* __strong _delegate;

    // GPU surface handle — written once in show(), then read-only (GPU-thread safe)
    std::atomic<void*>  _layerPtr{nullptr};

    PresentMode   _presentMode;
    bool          _shouldClose  = false;
    EventCallback  _eventCb;
    ResizeCallback _resizeCb;
    CloseCallback  _closeCb;

    // Previous modifier flags for tracking individual modifier key press/release
    NSEventModifierFlags _lastModFlags = 0;
};

} // namespace dexilate::platform

// ─────────────────────────────────────────────────────────────────────────────
//  DexilateMetalView implementation
// ─────────────────────────────────────────────────────────────────────────────
@implementation DexilateMetalView

+ (Class)layerClass { return [CAMetalLayer class]; }

- (BOOL)wantsLayer        { return YES; }
- (BOOL)isOpaque          { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)wantsUpdateLayer  { return YES; }

- (CALayer*)makeBackingLayer {
    CAMetalLayer* layer = [CAMetalLayer layer];
    layer.pixelFormat   = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;
    return layer;
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    if (self.window) {
        CGFloat dpr = self.window.backingScaleFactor;
        CAMetalLayer* ml = (CAMetalLayer*)self.layer;
        ml.contentsScale = dpr;
        CGSize sz = [self convertSizeToBacking:self.bounds.size];
        ml.drawableSize  = sz;
    }
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    if (self.window && self.layer) {
        CGFloat dpr = self.window.backingScaleFactor;
        CAMetalLayer* ml = (CAMetalLayer*)self.layer;
        ml.contentsScale = dpr;
        CGSize sz = [self convertSizeToBacking:self.bounds.size];
        ml.drawableSize  = sz;
    }
}

// ── Mouse events ──────────────────────────────────────────────────────────────
- (void)mouseMoved:(NSEvent*)e       { if (_cppWindow) _cppWindow->handleMouseMove(e); }
- (void)mouseDragged:(NSEvent*)e     { if (_cppWindow) _cppWindow->handleMouseMove(e); }
- (void)rightMouseDragged:(NSEvent*)e{ if (_cppWindow) _cppWindow->handleMouseMove(e); }
- (void)otherMouseDragged:(NSEvent*)e{ if (_cppWindow) _cppWindow->handleMouseMove(e); }

- (void)mouseDown:(NSEvent*)e  { if (_cppWindow) _cppWindow->handleMouseButton(e, true,  dexilate::platform::MouseButton::Left);   }
- (void)mouseUp:(NSEvent*)e    { if (_cppWindow) _cppWindow->handleMouseButton(e, false, dexilate::platform::MouseButton::Left);   }
- (void)rightMouseDown:(NSEvent*)e  { if (_cppWindow) _cppWindow->handleMouseButton(e, true,  dexilate::platform::MouseButton::Right);  }
- (void)rightMouseUp:(NSEvent*)e    { if (_cppWindow) _cppWindow->handleMouseButton(e, false, dexilate::platform::MouseButton::Right);  }
- (void)otherMouseDown:(NSEvent*)e  { if (_cppWindow) _cppWindow->handleMouseButton(e, true,  dexilate::platform::MouseButton::Middle); }
- (void)otherMouseUp:(NSEvent*)e    { if (_cppWindow) _cppWindow->handleMouseButton(e, false, dexilate::platform::MouseButton::Middle); }

- (void)scrollWheel:(NSEvent*)e  { if (_cppWindow) _cppWindow->handleScroll(e); }

// ── Keyboard events ───────────────────────────────────────────────────────────
- (void)keyDown:(NSEvent*)e {
    if (_cppWindow) _cppWindow->handleKeyDown(e);
    // Do NOT call [super keyDown:] — that would NSBeep on unhandled keys.
}
- (void)keyUp:(NSEvent*)e {
    if (_cppWindow) _cppWindow->handleKeyUp(e);
}
- (void)flagsChanged:(NSEvent*)e {
    if (_cppWindow) _cppWindow->handleFlagsChanged(e);
}

// ── Stylus / tablet events ────────────────────────────────────────────────────
- (void)tabletPoint:(NSEvent*)e {
    if (_cppWindow) _cppWindow->handleTabletPoint(e);
}

// Accept mouse-moved events even without a button held
- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    for (NSTrackingArea* ta in self.trackingAreas)
        [self removeTrackingArea:ta];
    NSTrackingArea* ta = [[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:(NSTrackingMouseMoved |
                      NSTrackingMouseEnteredAndExited |
                      NSTrackingActiveInKeyWindow |
                      NSTrackingInVisibleRect)
               owner:self
            userInfo:nil];
    [self addTrackingArea:ta];
}

@end

// ─────────────────────────────────────────────────────────────────────────────
//  DexilateWindowDelegate implementation
// ─────────────────────────────────────────────────────────────────────────────
@implementation DexilateWindowDelegate

- (BOOL)windowShouldClose:(NSWindow*)sender {
    if (_cppWindow) _cppWindow->onCloseRequested();
    // Return NO — we handle the close ourselves via shouldClose().
    return NO;
}

- (void)windowDidResize:(NSNotification*)note {
    if (!_cppWindow) return;
    NSWindow* w = (NSWindow*)note.object;
    NSRect r = w.contentView.bounds;
    _cppWindow->onResize((uint32_t)r.size.width, (uint32_t)r.size.height);
}

- (void)windowDidChangeBackingProperties:(NSNotification*)note {
    if (!_cppWindow) return;
    NSWindow* w = (NSWindow*)note.object;
    _cppWindow->onDpiChange((float)w.backingScaleFactor);
}

- (void)windowDidBecomeKey:(NSNotification*)__unused note {
    if (_cppWindow) _cppWindow->onFocus(true);
}

- (void)windowDidResignKey:(NSNotification*)__unused note {
    if (_cppWindow) _cppWindow->onFocus(false);
}

@end

// ─────────────────────────────────────────────────────────────────────────────
//  MacWindow — method implementations
// ─────────────────────────────────────────────────────────────────────────────
namespace dexilate::platform {

MacWindow::MacWindow(const WindowDesc& desc)
    : _presentMode(desc.presentMode)
{
    @autoreleasepool {
        // ── NSWindow ──────────────────────────────────────────────────────────
        NSWindowStyleMask style =
            NSWindowStyleMaskTitled          |
            NSWindowStyleMaskClosable        |
            NSWindowStyleMaskMiniaturizable  |
            (desc.resizable ? NSWindowStyleMaskResizable : 0);

        NSRect frame = NSMakeRect(0, 0, desc.width, desc.height);

        _window = [[NSWindow alloc]
            initWithContentRect:frame
                      styleMask:style
                        backing:NSBackingStoreBuffered
                          defer:NO];

        [_window setTitle:[NSString stringWithUTF8String:desc.title.c_str()]];
        [_window center];
        [_window setAcceptsMouseMovedEvents:YES];
        [_window setReleasedWhenClosed:NO];

        if (desc.maximized)  [_window zoom:nil];

        // ── DexilateMetalView ───────────────────────────────────────────────────
        _view = [[DexilateMetalView alloc] initWithFrame:frame];
        _view.cppWindow = this;

        [_window setContentView:_view];
        [_window makeFirstResponder:_view];

        // ── CAMetalLayer ──────────────────────────────────────────────────────
        _metalLayer = (CAMetalLayer*)_view.layer;
        applyPresentModeToLayer();

        // ── Delegate ──────────────────────────────────────────────────────────
        _delegate = [[DexilateWindowDelegate alloc] init];
        _delegate.cppWindow = this;
        [_window setDelegate:_delegate];

        if (desc.fullscreen)
            [_window toggleFullScreen:nil];
    }
}

MacWindow::~MacWindow() {
    @autoreleasepool {
        [_window setDelegate:nil];
        _delegate.cppWindow = nullptr;
        _view.cppWindow     = nullptr;
        [_window orderOut:nil];
        _window     = nil;
        _view       = nil;
        _metalLayer = nil;
        _delegate   = nil;
    }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void MacWindow::show() {
    @autoreleasepool {
        [_window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        // Store the CAMetalLayer pointer atomically so GPU thread can read it.
        _layerPtr.store((__bridge void*)_metalLayer, std::memory_order_release);
    }
}

void MacWindow::hide() {
    @autoreleasepool { [_window orderOut:nil]; }
}

void MacWindow::close() {
    _shouldClose = true;
    @autoreleasepool { [_window orderOut:nil]; }
}

void MacWindow::pollEvents() {
    @autoreleasepool {
        NSEvent* event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                          untilDate:[NSDate distantPast]
                                             inMode:NSDefaultRunLoopMode
                                            dequeue:YES]) != nil) {
            [NSApp sendEvent:event];
        }
        [NSApp updateWindows];
    }
}

bool MacWindow::shouldClose() const { return _shouldClose; }

// ── Dimensions ────────────────────────────────────────────────────────────────
uint32_t MacWindow::width() const {
    return (uint32_t)_view.bounds.size.width;
}
uint32_t MacWindow::height() const {
    return (uint32_t)_view.bounds.size.height;
}
uint32_t MacWindow::physicalWidth() const {
    double dw = _metalLayer.drawableSize.width;
    if (dw > 0) return (uint32_t)dw;
    // Fallback before first draw: logical size × backing scale factor
    return (uint32_t)(_view.bounds.size.width * _window.backingScaleFactor);
}
uint32_t MacWindow::physicalHeight() const {
    double dh = _metalLayer.drawableSize.height;
    if (dh > 0) return (uint32_t)dh;
    return (uint32_t)(_view.bounds.size.height * _window.backingScaleFactor);
}
float MacWindow::devicePixelRatio() const {
    return _window ? (float)_window.backingScaleFactor : 1.0f;
}

void MacWindow::resize(uint32_t w, uint32_t h) {
    @autoreleasepool {
        NSRect frame = [_window frameRectForContentRect:NSMakeRect(0, 0, w, h)];
        [_window setFrame:frame display:YES];
    }
}

// ── Title & state ─────────────────────────────────────────────────────────────
void MacWindow::setTitle(std::string_view t) {
    @autoreleasepool {
        [_window setTitle:[NSString stringWithUTF8String:std::string(t).c_str()]];
    }
}
std::string MacWindow::title() const {
    return std::string([_window.title UTF8String]);
}

void MacWindow::setFullscreen(bool fs) {
    bool current = isFullscreen();
    if (current != fs)
        [_window toggleFullScreen:nil];
}
bool MacWindow::isFullscreen() const {
    return (_window.styleMask & NSWindowStyleMaskFullScreen) != 0;
}

// ── GPU surface ───────────────────────────────────────────────────────────────
GpuSurfaceHandle MacWindow::gpuSurfaceHandle() const {
    // Load atomically — safe from any thread after show().
    return GpuSurfaceHandle{ _layerPtr.load(std::memory_order_acquire), nullptr };
}

void MacWindow::applyPresentModeToLayer() {
    // displaySyncEnabled maps to vsync on/off.
    // Mailbox behaves like Fifo on macOS (CAMetalLayer manages triple-buffering).
    _metalLayer.displaySyncEnabled = (_presentMode != PresentMode::Immediate);
}

void MacWindow::setPresentMode(PresentMode mode) {
    _presentMode = mode;
    applyPresentModeToLayer();
}
PresentMode MacWindow::presentMode() const { return _presentMode; }

// GPU thread safe — wgpu-native handles the drawable acquire/present lifecycle.
void MacWindow::present() { /* wgpu-native drives CAMetalLayer directly */ }

// ── Delegate callbacks ────────────────────────────────────────────────────────
void MacWindow::onResize(uint32_t w, uint32_t h) {
    if (_resizeCb) _resizeCb(w, h);
    WindowResizeEvent e{ w, h, devicePixelRatio() };
    dispatchEvent(e);
}

void MacWindow::onDpiChange(float dpr) {
    CAMetalLayer* ml = (CAMetalLayer*)_view.layer;
    ml.contentsScale = dpr;
    CGSize sz = [_view convertSizeToBacking:_view.bounds.size];
    ml.drawableSize = sz;
    WindowResizeEvent e{ width(), height(), dpr };
    dispatchEvent(e);
}

void MacWindow::onFocus(bool gained) {
    dispatchEvent(WindowFocusEvent{ gained });
}

void MacWindow::onCloseRequested() {
    _shouldClose = true;
    if (_closeCb) _closeCb();
}

// ── Event helpers ─────────────────────────────────────────────────────────────
float MacWindow::viewHeight() const {
    return (float)_view.bounds.size.height;
}

std::pair<float,float> MacWindow::flipY(CGPoint p) const {
    // Flip from macOS bottom-left to Dexilate top-left origin.
    return { (float)p.x, viewHeight() - (float)p.y };
}

// ── Mouse / pointer ───────────────────────────────────────────────────────────
void MacWindow::handleMouseMove(NSEvent* event) {
    auto [x, y] = flipY([event locationInWindow]);
    MouseMoveEvent e{
        x, y,
        (float)event.deltaX, -(float)event.deltaY,
        modifiersFromFlags(event.modifierFlags)
    };
    // If this NSEvent carries tablet data, dispatch a StylusEvent as well.
    if (event.subtype == NSEventSubtypeTabletPoint) {
        handleTabletPoint(event);
    } else {
        dispatchEvent(e);
    }
}

void MacWindow::handleMouseButton(NSEvent* event, bool pressed, MouseButton btn) {
    auto [x, y] = flipY([event locationInWindow]);
    MouseButtonEvent e{
        x, y, btn, pressed,
        (int)event.clickCount,
        modifiersFromFlags(event.modifierFlags)
    };
    dispatchEvent(e);
}

void MacWindow::handleScroll(NSEvent* event) {
    auto [x, y] = flipY([event locationInWindow]);
    ScrollEvent e{
        x, y,
        (float)event.scrollingDeltaX,
        (float)event.scrollingDeltaY,
        (bool)event.hasPreciseScrollingDeltas,
        modifiersFromFlags(event.modifierFlags)
    };
    dispatchEvent(e);
}

// ── Keyboard ──────────────────────────────────────────────────────────────────
void MacWindow::handleKeyDown(NSEvent* event) {
    KeyEvent ke{
        vkToKeyCode(event.keyCode),
        event.keyCode,
        true,
        (bool)event.isARepeat,
        modifiersFromFlags(event.modifierFlags)
    };
    dispatchEvent(ke);

    // Text input: emit for printable characters, skip control sequences.
    NSString* chars = event.characters;
    if (chars.length > 0) {
        unichar ch = [chars characterAtIndex:0];
        // Skip control characters (0x00–0x1F, 0x7F) and private-use area.
        if (ch >= 0x20 && ch != 0x7F) {
            // Convert UTF-16 to UTF-32 codepoint (handle surrogate pairs).
            char32_t cp = ch;
            if (ch >= 0xD800 && ch < 0xDC00 && chars.length >= 2) {
                unichar low = [chars characterAtIndex:1];
                cp = 0x10000 + ((char32_t(ch) - 0xD800) << 10) + (low - 0xDC00);
            }
            dispatchEvent(TextInputEvent{ cp });
        }
    }
}

void MacWindow::handleKeyUp(NSEvent* event) {
    KeyEvent ke{
        vkToKeyCode(event.keyCode),
        event.keyCode,
        false,
        false,
        modifiersFromFlags(event.modifierFlags)
    };
    dispatchEvent(ke);
}

void MacWindow::handleFlagsChanged(NSEvent* event) {
    // Emit synthetic KeyDown/Up for individual modifier keys as they change.
    NSEventModifierFlags cur  = event.modifierFlags;
    NSEventModifierFlags prev = _lastModFlags;
    _lastModFlags = cur;

    struct { NSEventModifierFlags mask; KeyCode code; } entries[] = {
        { NSEventModifierFlagShift,   KeyCode::ShiftLeft   },
        { NSEventModifierFlagControl, KeyCode::CtrlLeft    },
        { NSEventModifierFlagOption,  KeyCode::AltLeft     },
        { NSEventModifierFlagCommand, KeyCode::MetaLeft    },
        { NSEventModifierFlagCapsLock,KeyCode::Unknown     }, // handled as modifier only
    };

    Modifiers mods = modifiersFromFlags(cur);
    for (auto& e : entries) {
        if (e.code == KeyCode::Unknown) continue;
        bool wasDown = (prev & e.mask) != 0;
        bool isDown  = (cur  & e.mask) != 0;
        if (wasDown != isDown) {
            KeyEvent ke{ e.code, (uint32_t)event.keyCode, isDown, false, mods };
            dispatchEvent(ke);
        }
    }
}

// ── Stylus / tablet ───────────────────────────────────────────────────────────
void MacWindow::handleTabletPoint(NSEvent* event) {
    auto [x, y] = flipY([event locationInWindow]);
    NSPoint tilt = event.tilt;
    StylusEvent e{
        x, y,
        (float)event.pressure,
        (float)(tilt.x * 90.0f),   // [-1,+1] → [-90°,+90°]
        (float)(tilt.y * 90.0f),
        (float)(event.rotation),
        event.pointingDeviceType == NSPointingDeviceTypeEraser,
        true,   // inProximity — NSEventSubtypeTabletPoint means active
        modifiersFromFlags(event.modifierFlags)
    };
    dispatchEvent(e);
}

// ─────────────────────────────────────────────────────────────────────────────
//  IWindow factory
// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<IWindow> IWindow::create(const WindowDesc& desc) {
    @autoreleasepool {
        // Initialise NSApplication if no host framework (e.g. Qt) has done so.
        if (NSApp == nil) {
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            [NSApp finishLaunching];
        }
        return std::make_unique<MacWindow>(desc);
    }
}

} // namespace dexilate::platform
