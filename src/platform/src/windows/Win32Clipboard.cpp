// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/platform/src/windows/Win32Clipboard.cpp
//  IClipboard implementation for Windows
//
//  Image:    Write: RGBA8 → BITMAPV4HEADER DIB (CF_DIBV5) — preserves alpha.
//            Read:  CF_DIBV5 or CF_DIB → RGBA8; bottom-up row order corrected.
//  Text:     CF_UNICODETEXT (UTF-16) ↔ UTF-8 std::string.
//  SVG path: Dual-format — custom registered format (UTF-8 blob) +
//            CF_UNICODETEXT so external apps receive plain SVG markup.
//
//  All clipboard operations must occur on the main thread.
// ─────────────────────────────────────────────────────────────────────────────

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "dexilate/platform/IClipboard.h"

#include <cstring>

namespace dexilate::platform {

namespace {

// ── RAII for OpenClipboard / CloseClipboard ───────────────────────────────────
struct ClipboardGuard {
    bool opened;
    explicit ClipboardGuard(HWND hwnd = nullptr)
        : opened(OpenClipboard(hwnd) != FALSE) {}
    ~ClipboardGuard() { if (opened) CloseClipboard(); }
    explicit operator bool() const { return opened; }
};

// ── RAII for GlobalAlloc / GlobalLock ────────────────────────────────────────
// After SetClipboardData the OS owns the handle; call release() beforehand.
struct GlobalMem {
    HGLOBAL h   = nullptr;
    void*   ptr = nullptr;

    GlobalMem(size_t bytes, UINT flags = GMEM_MOVEABLE) {
        h = GlobalAlloc(flags, bytes);
        if (h) ptr = GlobalLock(h);
    }
    ~GlobalMem() {
        if (h) { GlobalUnlock(h); GlobalFree(h); }
    }

    // Transfer ownership to Windows (caller must not free after this)
    HGLOBAL release() {
        GlobalUnlock(h);
        HGLOBAL tmp = h;
        h = nullptr; ptr = nullptr;
        return tmp;
    }

    explicit operator bool() const { return ptr != nullptr; }
};

// ── Encoding helpers ──────────────────────────────────────────────────────────
std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
        static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
        static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string toUtf8(const wchar_t* w, size_t len) {
    if (!w || len == 0) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(len),
        nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(len),
        s.data(), n, nullptr, nullptr);
    return s;
}

// ── Custom clipboard format (registered once per process) ─────────────────────
UINT svgPathFormat() {
    static UINT fmt = RegisterClipboardFormatW(L"com.dexilate.svg-path");
    return fmt;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
class Win32Clipboard final : public IClipboard {
public:
    // ── Availability ──────────────────────────────────────────────────────────
    bool hasImage() const override {
        return IsClipboardFormatAvailable(CF_DIBV5) ||
               IsClipboardFormatAvailable(CF_DIB);
    }

    bool hasText() const override {
        return IsClipboardFormatAvailable(CF_UNICODETEXT) != 0;
    }

    bool hasSvgPath() const override {
        return IsClipboardFormatAvailable(svgPathFormat()) != 0;
    }

    // ── Write ─────────────────────────────────────────────────────────────────
    void setImage(const ClipboardImage& image) override {
        // CF_DIBV5 with BITMAPV4HEADER supports a 32-bit alpha channel.
        size_t pixBytes   = static_cast<size_t>(image.width) * image.height * 4;
        size_t totalBytes = sizeof(BITMAPV4HEADER) + pixBytes;

        GlobalMem mem(totalBytes);
        if (!mem) return;

        auto* hdr            = static_cast<BITMAPV4HEADER*>(mem.ptr);
        hdr->bV4Size          = sizeof(BITMAPV4HEADER);
        hdr->bV4Width         = static_cast<LONG>(image.width);
        hdr->bV4Height        = -static_cast<LONG>(image.height); // top-down
        hdr->bV4Planes        = 1;
        hdr->bV4BitCount      = 32;
        hdr->bV4V4Compression = BI_BITFIELDS;
        hdr->bV4SizeImage     = static_cast<DWORD>(pixBytes);
        // GDI expects BGRA channel order
        hdr->bV4RedMask       = 0x00FF0000u;
        hdr->bV4GreenMask     = 0x0000FF00u;
        hdr->bV4BlueMask      = 0x000000FFu;
        hdr->bV4AlphaMask     = 0xFF000000u;

        // Convert RGBA → BGRA in-place into the global memory block
        uint8_t*       dst = static_cast<uint8_t*>(mem.ptr) + sizeof(BITMAPV4HEADER);
        const uint8_t* src = image.pixels.data();
        size_t npix = static_cast<size_t>(image.width) * image.height;
        for (size_t i = 0; i < npix; ++i, src += 4, dst += 4) {
            dst[0] = src[2];  // B ← R
            dst[1] = src[1];  // G
            dst[2] = src[0];  // R ← B
            dst[3] = src[3];  // A
        }

        ClipboardGuard guard;
        if (!guard) return;
        EmptyClipboard();
        SetClipboardData(CF_DIBV5, mem.release());
    }

    void setText(const std::string& text) override {
        std::wstring wt    = toWide(text);
        size_t       bytes = (wt.size() + 1) * sizeof(wchar_t);
        GlobalMem    mem(bytes);
        if (!mem) return;
        std::memcpy(mem.ptr, wt.c_str(), bytes);

        ClipboardGuard guard;
        if (!guard) return;
        EmptyClipboard();
        SetClipboardData(CF_UNICODETEXT, mem.release());
    }

    void setSvgPath(const std::string& svgPathData) override {
        // Allocate both blobs before opening clipboard (avoids partial writes)
        GlobalMem customMem(svgPathData.size() + 1);
        if (!customMem) return;
        std::memcpy(customMem.ptr, svgPathData.c_str(), svgPathData.size() + 1);

        std::wstring wt    = toWide(svgPathData);
        size_t       bytes = (wt.size() + 1) * sizeof(wchar_t);
        GlobalMem    textMem(bytes);
        if (!textMem) return;
        std::memcpy(textMem.ptr, wt.c_str(), bytes);

        ClipboardGuard guard;
        if (!guard) return;
        EmptyClipboard();
        // Custom format for lossless Dexilate-to-Dexilate; CF_UNICODETEXT for
        // Inkscape, Affinity Designer, and other SVG-aware apps.
        SetClipboardData(svgPathFormat(), customMem.release());
        SetClipboardData(CF_UNICODETEXT, textMem.release());
    }

    // ── Read ──────────────────────────────────────────────────────────────────
    std::optional<ClipboardImage> getImage() const override {
        ClipboardGuard guard;
        if (!guard) return std::nullopt;

        HGLOBAL h = GetClipboardData(CF_DIBV5);
        if (!h)   h = GetClipboardData(CF_DIB);
        if (!h)   return std::nullopt;

        const auto* bmi = static_cast<const BITMAPINFOHEADER*>(GlobalLock(h));
        if (!bmi)  return std::nullopt;

        uint32_t w     = static_cast<uint32_t>(bmi->biWidth);
        int32_t  hRaw  = bmi->biHeight;
        uint32_t pixH  = static_cast<uint32_t>(hRaw < 0 ? -hRaw : hRaw);
        bool     topDn = hRaw < 0;

        // Only handle 32bpp bitmaps — the common case for CF_DIBV5
        if (bmi->biBitCount != 32 || pixH == 0) {
            GlobalUnlock(h);
            return std::nullopt;
        }

        ClipboardImage out;
        out.width  = w;
        out.height = pixH;
        out.pixels.resize(static_cast<size_t>(w) * pixH * 4);

        const uint8_t* src = reinterpret_cast<const uint8_t*>(bmi) + bmi->biSize;
        for (uint32_t row = 0; row < pixH; ++row) {
            // Bottom-up bitmaps (positive biHeight): row 0 is the bottom row
            uint32_t srcRow  = topDn ? row : (pixH - 1u - row);
            const uint8_t* rowSrc = src + srcRow * static_cast<size_t>(w) * 4;
            uint8_t*       rowDst =
                out.pixels.data() + row * static_cast<size_t>(w) * 4;
            for (uint32_t col = 0; col < w; ++col, rowSrc += 4, rowDst += 4) {
                rowDst[0] = rowSrc[2];  // R ← B
                rowDst[1] = rowSrc[1];  // G
                rowDst[2] = rowSrc[0];  // B ← R
                rowDst[3] = rowSrc[3];  // A
            }
        }
        GlobalUnlock(h);
        return out;
    }

    std::optional<std::string> getText() const override {
        ClipboardGuard guard;
        if (!guard) return std::nullopt;
        HGLOBAL h = GetClipboardData(CF_UNICODETEXT);
        if (!h)   return std::nullopt;
        const wchar_t* w = static_cast<const wchar_t*>(GlobalLock(h));
        if (!w)   return std::nullopt;
        std::string result = toUtf8(w, wcslen(w));
        GlobalUnlock(h);
        return result;
    }

    std::optional<std::string> getSvgPath() const override {
        ClipboardGuard guard;
        if (!guard) return std::nullopt;

        // Prefer our custom format (Dexilate-to-Dexilate, stored as UTF-8)
        HGLOBAL h = GetClipboardData(svgPathFormat());
        if (h) {
            const char* s = static_cast<const char*>(GlobalLock(h));
            if (s) {
                std::string result(s);
                GlobalUnlock(h);
                return result;
            }
        }
        // Fall back to Unicode text from external apps
        h = GetClipboardData(CF_UNICODETEXT);
        if (!h) return std::nullopt;
        const wchar_t* w = static_cast<const wchar_t*>(GlobalLock(h));
        if (!w) return std::nullopt;
        std::string result = toUtf8(w, wcslen(w));
        GlobalUnlock(h);
        return result;
    }

    // ── Clear ─────────────────────────────────────────────────────────────────
    void clear() override {
        ClipboardGuard guard;
        if (guard) EmptyClipboard();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<IClipboard> IClipboard::create() {
    return std::make_unique<Win32Clipboard>();
}

} // namespace dexilate::platform
