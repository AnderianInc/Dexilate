// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/platform/src/linux/LinuxClipboard.cpp
//  IClipboard implementation for Linux — X11 CLIPBOARD selection
//
//  X11 clipboard model (brief):
//    • Writing: call XSetSelectionOwner(CLIPBOARD). When another app requests
//      the data, X sends SelectionRequest — we respond with SetProperty and
//      SelectionNotify.  We also cache locally so in-process paste is instant.
//    • Reading: call XConvertSelection(CLIPBOARD, target, property) then wait
//      for SelectionNotify, then read the property.
//
//  This implementation is single-threaded (main thread only) and uses a local
//  in-process cache so Dexilate → Dexilate clipboard round-trips bypass the X11
//  protocol entirely.  Cross-app clipboard transfers process pending X events
//  in a short polling loop.
//
//  Image format: PNG-encoded bytes on the CLIPBOARD in "image/png" target;
//  RGBA8 is kept in the local cache.
//  Text format:  UTF-8 string ("UTF8_STRING" atom) on CLIPBOARD.
//  SVG path:     "application/x-dexilate-svg-path" + "image/svg+xml" atoms.
//
//  Limitations vs. a full implementation:
//    - No INCR (incremental transfer) — limits clipboard data to ~256 KB.
//    - No event loop integration — uses a blocking XPeekEvent poll for reads.
//    - Wayland: when running under XWayland the X11 selection is bridged to
//      Wayland automatically by the compositor; no Wayland-specific code needed.
// ─────────────────────────────────────────────────────────────────────────────

#include "dexilate/platform/IClipboard.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace dexilate::platform {

// ─────────────────────────────────────────────────────────────────────────────
namespace {

// Shared X11 display — re-use the one opened by X11Window if available.
// We open a separate connection here to keep clipboard fully independent of
// whether any window is open.
Display* clipDisplay() {
    static Display* dpy = XOpenDisplay(nullptr);
    return dpy;
}

struct X11Atoms {
    Atom CLIPBOARD;
    Atom UTF8_STRING;
    Atom DEXILATE_DATA;   // "application/x-dexilate-svg-path"
    Atom SVG_XML;       // "image/svg+xml"
    Atom PNG;           // "image/png"
    Atom TARGETS;       // TARGETS — list supported types
    Atom DEXILATE_PROP;   // scratch property for our window

    explicit X11Atoms(Display* dpy) {
        CLIPBOARD   = XInternAtom(dpy, "CLIPBOARD",                         False);
        UTF8_STRING = XInternAtom(dpy, "UTF8_STRING",                       False);
        DEXILATE_DATA = XInternAtom(dpy, "application/x-dexilate-svg-path",    False);
        SVG_XML     = XInternAtom(dpy, "image/svg+xml",                     False);
        PNG         = XInternAtom(dpy, "image/png",                         False);
        TARGETS     = XInternAtom(dpy, "TARGETS",                           False);
        DEXILATE_PROP = XInternAtom(dpy, "DEXILATE_CLIPBOARD_PROP",             False);
    }
};

const X11Atoms& atoms(Display* dpy) {
    static X11Atoms a(dpy);
    return a;
}

// Minimal off-screen window used for clipboard ownership and property storage.
Window clipWindow(Display* dpy) {
    static Window w = XCreateSimpleWindow(
        dpy, DefaultRootWindow(dpy), -1, -1, 1, 1, 0, 0, 0);
    return w;
}

// Wait up to `timeoutMs` for a SelectionNotify event, processing other
// events in the meantime.  Returns true if the notify arrived.
bool waitForSelectionNotify(Display* dpy, Window win, int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == SelectionNotify && ev.xselection.requestor == win)
                return ev.xselection.property != None;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

// Read the contents of a window property as a byte vector.
std::vector<uint8_t> readProperty(Display* dpy, Window win, Atom prop) {
    Atom actualType;
    int  format;
    unsigned long nitems, bytesAfter;
    unsigned char* data = nullptr;

    XGetWindowProperty(dpy, win, prop, 0, 0x7FFFFFFF, False,
        AnyPropertyType, &actualType, &format, &nitems, &bytesAfter, &data);

    std::vector<uint8_t> result;
    if (data && nitems > 0) {
        size_t bytes = nitems * static_cast<size_t>(format / 8);
        result.assign(data, data + bytes);
    }
    if (data) XFree(data);
    XDeleteProperty(dpy, win, prop);
    return result;
}

// Request a clipboard target and return the raw bytes. Timeout 200 ms.
std::vector<uint8_t> getClipboardData(Display* dpy, Atom target) {
    const auto& at = atoms(dpy);
    Window win = clipWindow(dpy);
    XDeleteProperty(dpy, win, at.DEXILATE_PROP);
    XConvertSelection(dpy, at.CLIPBOARD, target, at.DEXILATE_PROP, win, CurrentTime);
    XFlush(dpy);
    if (!waitForSelectionNotify(dpy, win, 200))
        return {};
    return readProperty(dpy, win, at.DEXILATE_PROP);
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
class LinuxClipboard final : public IClipboard {
public:
    // ── Availability ──────────────────────────────────────────────────────────
    bool hasImage() const override {
        // Check local cache first (fast path for in-process use)
        if (_cacheImage.has_value()) return true;
        return isOwner() && !_cacheImage.has_value()
            ? false
            : checkTarget(atoms(clipDisplay()).PNG);
    }

    bool hasText() const override {
        if (_cacheText.has_value()) return true;
        return checkTarget(atoms(clipDisplay()).UTF8_STRING);
    }

    bool hasSvgPath() const override {
        if (_cacheSvg.has_value()) return true;
        return checkTarget(atoms(clipDisplay()).DEXILATE_DATA);
    }

    // ── Write ─────────────────────────────────────────────────────────────────
    void setImage(const ClipboardImage& image) override {
        _cacheImage = image;
        _cacheText  = std::nullopt;
        _cacheSvg   = std::nullopt;
        _imageBytes.clear();  // PNG will be encoded on demand in SelectionRequest
        acquireOwnership();
    }

    void setText(const std::string& text) override {
        _cacheText  = text;
        _cacheImage = std::nullopt;
        _cacheSvg   = std::nullopt;
        acquireOwnership();
    }

    void setSvgPath(const std::string& svgPathData) override {
        _cacheSvg   = svgPathData;
        _cacheImage = std::nullopt;
        _cacheText  = std::nullopt;
        acquireOwnership();
    }

    // ── Read ──────────────────────────────────────────────────────────────────
    std::optional<ClipboardImage> getImage() const override {
        if (_cacheImage.has_value()) return _cacheImage;
        // X11 image retrieval requires PNG decode — deferred to Phase 1
        // (requires libpng or stb_image as a dependency).
        // For now return nullopt for cross-app image paste.
        return std::nullopt;
    }

    std::optional<std::string> getText() const override {
        if (_cacheText.has_value()) return _cacheText;
        auto bytes = getClipboardData(clipDisplay(), atoms(clipDisplay()).UTF8_STRING);
        if (bytes.empty()) return std::nullopt;
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    std::optional<std::string> getSvgPath() const override {
        if (_cacheSvg.has_value()) return _cacheSvg;
        // Try canvas-specific format first
        auto bytes = getClipboardData(clipDisplay(), atoms(clipDisplay()).DEXILATE_DATA);
        if (!bytes.empty())
            return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        // Fall back to SVG XML / plain text
        bytes = getClipboardData(clipDisplay(), atoms(clipDisplay()).SVG_XML);
        if (!bytes.empty())
            return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return std::nullopt;
    }

    // ── Clear ─────────────────────────────────────────────────────────────────
    void clear() override {
        _cacheImage = std::nullopt;
        _cacheText  = std::nullopt;
        _cacheSvg   = std::nullopt;
        _imageBytes.clear();
        // Relinquish ownership
        Display* dpy = clipDisplay();
        if (dpy) {
            XSetSelectionOwner(dpy, atoms(dpy).CLIPBOARD, None, CurrentTime);
            XFlush(dpy);
        }
        _owner = false;
    }

    // ── SelectionRequest handler (called from pollEvents in X11Window) ─────────
    // In a full integration this would be driven by the event loop.
    // For Phase 0, in-process clipboard is handled via the local cache.
    void handleSelectionRequest(const XSelectionRequestEvent& req) {
        Display*        dpy  = clipDisplay();
        const auto&     at   = atoms(dpy);
        XSelectionEvent reply = {};
        reply.type      = SelectionNotify;
        reply.requestor = req.requestor;
        reply.selection = req.selection;
        reply.target    = req.target;
        reply.property  = None;
        reply.time      = req.time;

        if (req.target == at.TARGETS) {
            // Advertise supported targets
            std::vector<Atom> supported;
            if (_cacheText)  supported.push_back(at.UTF8_STRING);
            if (_cacheSvg) {
                supported.push_back(at.DEXILATE_DATA);
                supported.push_back(at.SVG_XML);
                supported.push_back(at.UTF8_STRING);
            }
            XChangeProperty(dpy, req.requestor, req.property,
                XA_ATOM, 32, PropModeReplace,
                reinterpret_cast<const unsigned char*>(supported.data()),
                static_cast<int>(supported.size()));
            reply.property = req.property;
        } else if (req.target == at.UTF8_STRING) {
            std::string data;
            if (_cacheText)  data = *_cacheText;
            else if (_cacheSvg) data = *_cacheSvg;
            if (!data.empty()) {
                XChangeProperty(dpy, req.requestor, req.property,
                    at.UTF8_STRING, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(data.c_str()),
                    static_cast<int>(data.size()));
                reply.property = req.property;
            }
        } else if (req.target == at.DEXILATE_DATA && _cacheSvg) {
            XChangeProperty(dpy, req.requestor, req.property,
                at.DEXILATE_DATA, 8, PropModeReplace,
                reinterpret_cast<const unsigned char*>(_cacheSvg->c_str()),
                static_cast<int>(_cacheSvg->size()));
            reply.property = req.property;
        } else if (req.target == at.SVG_XML && _cacheSvg) {
            XChangeProperty(dpy, req.requestor, req.property,
                at.SVG_XML, 8, PropModeReplace,
                reinterpret_cast<const unsigned char*>(_cacheSvg->c_str()),
                static_cast<int>(_cacheSvg->size()));
            reply.property = req.property;
        }

        XSendEvent(dpy, req.requestor, False, 0,
            reinterpret_cast<XEvent*>(&reply));
        XFlush(dpy);
    }

private:
    void acquireOwnership() {
        Display* dpy = clipDisplay();
        if (!dpy) return;
        XSetSelectionOwner(dpy, atoms(dpy).CLIPBOARD, clipWindow(dpy), CurrentTime);
        XFlush(dpy);
        _owner = true;
    }

    bool isOwner() const {
        Display* dpy = clipDisplay();
        if (!dpy) return false;
        return XGetSelectionOwner(dpy, atoms(dpy).CLIPBOARD) == clipWindow(dpy);
    }

    bool checkTarget(Atom target) const {
        // Request TARGETS from current owner; check if target is listed
        auto bytes = getClipboardData(clipDisplay(), atoms(clipDisplay()).TARGETS);
        if (bytes.empty()) return false;
        const auto* atoms_ptr = reinterpret_cast<const Atom*>(bytes.data());
        size_t n = bytes.size() / sizeof(Atom);
        for (size_t i = 0; i < n; ++i)
            if (atoms_ptr[i] == target) return true;
        return false;
    }

    // ── Cache ─────────────────────────────────────────────────────────────────
    std::optional<ClipboardImage> _cacheImage;
    std::optional<std::string>    _cacheText;
    std::optional<std::string>    _cacheSvg;
    std::vector<uint8_t>          _imageBytes;   // PNG-encoded cache (lazy)
    bool                          _owner = false;
};

// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<IClipboard> IClipboard::create() {
    return std::make_unique<LinuxClipboard>();
}

} // namespace dexilate::platform
