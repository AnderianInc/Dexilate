// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/engine/raster/Compositor.h"
#include "dexilate/engine/document/Document.h"

#include <cstring>
#include <cmath>
#include <algorithm>
#include <array>

namespace dexilate::engine {

// ── HSL helpers ────────────────────────────────────────────────────────────────

namespace {

struct HSL { float h, s, l; };

static float hueToRgb(float p, float q, float t) noexcept {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

static HSL rgbToHsl(float r, float g, float b) noexcept {
    float maxC = std::max({r, g, b});
    float minC = std::min({r, g, b});
    float l = (maxC + minC) * 0.5f;
    if (maxC == minC) return {0.0f, 0.0f, l};

    float d = maxC - minC;
    float s = (l > 0.5f) ? d / (2.0f - maxC - minC) : d / (maxC + minC);
    float h;
    if      (maxC == r) h = (g - b) / d + (g < b ? 6.0f : 0.0f);
    else if (maxC == g) h = (b - r) / d + 2.0f;
    else                h = (r - g) / d + 4.0f;
    h /= 6.0f;
    return {h, s, l};
}

static std::array<float, 3> hslToRgb(HSL hsl) noexcept {
    if (hsl.s == 0.0f)
        return {hsl.l, hsl.l, hsl.l};
    float q = (hsl.l < 0.5f) ? hsl.l * (1.0f + hsl.s)
                              : hsl.l + hsl.s - hsl.l * hsl.s;
    float p = 2.0f * hsl.l - q;
    return { hueToRgb(p, q, hsl.h + 1.0f / 3.0f),
             hueToRgb(p, q, hsl.h),
             hueToRgb(p, q, hsl.h - 1.0f / 3.0f) };
}

static uint8_t clampByte(float v) noexcept {
    return static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, v + 0.5f)));
}

// Apply blend mode to a single RGB channel (returns float 0–255).
// For HSL modes the caller handles all three channels together.
static float blendChannel(float dst, float src, BlendMode mode) noexcept {
    switch (mode) {
    case BlendMode::Normal:
        return src;
    case BlendMode::Multiply:
        return dst * src / 255.0f;
    case BlendMode::Screen:
        return 255.0f - (255.0f - dst) * (255.0f - src) / 255.0f;
    case BlendMode::Overlay:
        return (dst < 128.0f)
            ? 2.0f * dst * src / 255.0f
            : 255.0f - 2.0f * (255.0f - dst) * (255.0f - src) / 255.0f;
    case BlendMode::Darken:
        return std::min(dst, src);
    case BlendMode::Lighten:
        return std::max(dst, src);
    case BlendMode::ColorDodge:
        return std::min(255.0f, dst * 255.0f / std::max(1.0f, 255.0f - src));
    case BlendMode::ColorBurn:
        return 255.0f - std::min(255.0f, (255.0f - dst) * 255.0f / std::max(1.0f, src));
    case BlendMode::HardLight:
        // Overlay with src/dst swapped
        return (src < 128.0f)
            ? 2.0f * dst * src / 255.0f
            : 255.0f - 2.0f * (255.0f - dst) * (255.0f - src) / 255.0f;
    case BlendMode::SoftLight: {
        // W3C formula (straight alpha, 0–255 range)
        float d = dst / 255.0f;
        float s = src / 255.0f;
        float result;
        if (s <= 0.5f) {
            result = d - (1.0f - 2.0f * s) * d * (1.0f - d);
        } else {
            float g = (d <= 0.25f) ? ((16.0f * d - 12.0f) * d + 4.0f) * d
                                   : std::sqrt(d);
            result = d + (2.0f * s - 1.0f) * (g - d);
        }
        return result * 255.0f;
    }
    case BlendMode::Difference:
        return std::abs(dst - src);
    case BlendMode::Exclusion:
        return dst + src - 2.0f * dst * src / 255.0f;
    default:
        return src; // HSL modes handled separately
    }
}

} // anonymous namespace

// ── Compositor::blendPixel ─────────────────────────────────────────────────────

void Compositor::blendPixel(uint8_t* dst, const uint8_t* src,
                             BlendMode mode, float opacity) noexcept {
    float srcA = (src[3] / 255.0f) * opacity;
    if (srcA <= 0.0f) return;

    float dstA = dst[3] / 255.0f;

    float blendedR, blendedG, blendedB;

    // HSL modes operate on the full RGB triple.
    if (mode == BlendMode::Hue || mode == BlendMode::Saturation ||
        mode == BlendMode::Color || mode == BlendMode::Luminosity) {

        HSL dstHSL = rgbToHsl(dst[0] / 255.0f, dst[1] / 255.0f, dst[2] / 255.0f);
        HSL srcHSL = rgbToHsl(src[0] / 255.0f, src[1] / 255.0f, src[2] / 255.0f);

        HSL resultHSL = dstHSL;
        switch (mode) {
        case BlendMode::Hue:
            resultHSL.h = srcHSL.h;
            break;
        case BlendMode::Saturation:
            resultHSL.s = srcHSL.s;
            break;
        case BlendMode::Color:
            resultHSL.h = srcHSL.h;
            resultHSL.s = srcHSL.s;
            break;
        case BlendMode::Luminosity:
            resultHSL.l = srcHSL.l;
            break;
        default: break;
        }

        auto rgb = hslToRgb(resultHSL);
        blendedR = rgb[0] * 255.0f;
        blendedG = rgb[1] * 255.0f;
        blendedB = rgb[2] * 255.0f;
    } else {
        blendedR = blendChannel(dst[0], src[0], mode);
        blendedG = blendChannel(dst[1], src[1], mode);
        blendedB = blendChannel(dst[2], src[2], mode);
    }

    // Porter-Duff src-over with the blended colour as the source.
    float outA = srcA + dstA * (1.0f - srcA);
    if (outA <= 0.0f) return;

    float inv = 1.0f / outA;
    float t   = dstA * (1.0f - srcA);

    dst[0] = clampByte((blendedR * srcA + dst[0] * t) * inv);
    dst[1] = clampByte((blendedG * srcA + dst[1] * t) * inv);
    dst[2] = clampByte((blendedB * srcA + dst[2] * t) * inv);
    dst[3] = clampByte(outA * 255.0f);
}

// ── Compositor::composite ──────────────────────────────────────────────────────

void Compositor::composite(const Document& doc, uint8_t* out, uint32_t w, uint32_t h) {
    std::memset(out, 0, static_cast<size_t>(w) * h * 4);

    std::vector<uint8_t> layerBuf(static_cast<size_t>(w) * h * 4);
    size_t total = static_cast<size_t>(w) * h;

    for (int i = 0; i < doc.layerCount(); ++i) {
        const Layer* layer = doc.layerAt(i);
        if (!layer || !layer->visible) continue;

        std::fill(layerBuf.begin(), layerBuf.end(), 0);

        if (const auto* raster = dynamic_cast<const RasterLayer*>(layer)) {
            raster->composite(layerBuf.data(), w, h);
        } else if (const auto* group = dynamic_cast<const GroupLayer*>(layer)) {
            group->composite(layerBuf.data(), w, h);
        } else if (const auto* adj = dynamic_cast<const AdjustmentLayer*>(layer)) {
            // Copy the current composite state, apply the adjustment in-place.
            std::memcpy(layerBuf.data(), out, total * 4);
            adj->apply(layerBuf.data(), w, h);
            // Replace output directly — adjustments modify the whole stack below.
            std::memcpy(out, layerBuf.data(), total * 4);
            continue;
        } else {
            continue;
        }

        for (size_t px = 0; px < total; ++px) {
            blendPixel(out + px * 4, layerBuf.data() + px * 4,
                       layer->blendMode, layer->opacity);
        }
    }
}

} // namespace dexilate::engine
