// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  dexilate/platform/IClipboard.h
//
//  Platform-agnostic clipboard abstraction (Layer 2 PAL).
//
//  Supports copying and pasting images (raw RGBA pixels), SVG path data,
//  and plain text between Dexilate and other applications.
//
//  Thread safety: All methods must be called from the main thread.
//  The OS clipboard is a shared, main-thread resource on all platforms.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dexilate::platform {

// ── Clipboard image payload ───────────────────────────────────────────────────
struct ClipboardImage {
    std::vector<uint8_t> pixels; ///< RGBA8 pixel data, row-major, no padding
    uint32_t             width;
    uint32_t             height;
};

// ─────────────────────────────────────────────────────────────────────────────
//  IClipboard
// ─────────────────────────────────────────────────────────────────────────────
class IClipboard {
public:
    virtual ~IClipboard() = default;

    // ── Availability checks ───────────────────────────────────────────────────
    [[nodiscard]] virtual bool hasImage()   const = 0;
    [[nodiscard]] virtual bool hasText()    const = 0;
    [[nodiscard]] virtual bool hasSvgPath() const = 0;

    // ── Write ─────────────────────────────────────────────────────────────────
    /// Place an RGBA image on the clipboard. Implementations convert to the
    /// native image format (PNG on macOS/Linux, DIB on Windows) before writing.
    virtual void setImage(const ClipboardImage& image) = 0;

    /// Place plain UTF-8 text on the clipboard.
    virtual void setText(const std::string& text) = 0;

    /// Place SVG path data on the clipboard. Stored as both:
    ///   - a custom "com.dexilate.svg-path" UTI for Dexilate-to-Dexilate paste
    ///   - plain SVG text so other vector apps (Inkscape, Affinity) can paste it
    virtual void setSvgPath(const std::string& svgPathData) = 0;

    // ── Read ──────────────────────────────────────────────────────────────────
    /// Returns the clipboard image decoded to RGBA8, or nullopt if none.
    /// Note: macOS decodes to premultiplied-alpha RGBA8.
    [[nodiscard]] virtual std::optional<ClipboardImage> getImage()   const = 0;

    /// Returns clipboard text, or nullopt if none.
    [[nodiscard]] virtual std::optional<std::string>    getText()    const = 0;

    /// Returns SVG path data, or nullopt if none.
    /// Prefers the "com.dexilate.svg-path" UTI; falls back to plain SVG text.
    [[nodiscard]] virtual std::optional<std::string>    getSvgPath() const = 0;

    // ── Clear ─────────────────────────────────────────────────────────────────
    virtual void clear() = 0;

    // ── Factory ───────────────────────────────────────────────────────────────
    [[nodiscard]] static std::unique_ptr<IClipboard> create();
};

} // namespace dexilate::platform
