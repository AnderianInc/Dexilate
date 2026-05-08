// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "dexilate/engine/raster/TileManager.h"
#include "dexilate/core/Tile.h"

#include <memory>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace dexilate::engine {

// ISO blend modes — Normal is the only one used in Phase 1.
enum class BlendMode : uint8_t {
    Normal = 0,
    Multiply, Screen, Overlay,
    Darken, Lighten,
    ColorDodge, ColorBurn,
    HardLight, SoftLight,
    Difference, Exclusion,
    Hue, Saturation, Color, Luminosity
};

// ── Base layer ─────────────────────────────────────────────────────────────────
class Layer {
public:
    virtual ~Layer() = default;

    std::string name      = "Layer";
    bool        visible   = true;
    float       opacity   = 1.0f;
    BlendMode   blendMode = BlendMode::Normal;
};

// ── Raster layer ───────────────────────────────────────────────────────────────
// Owns a TileManager that stores the pixel data. Coordinates are in document
// space (top-left = 0,0). All painting is CPU-side; GPU upload is done by
// TileUploader in the render loop.
class RasterLayer : public Layer {
public:
    RasterLayer(uint32_t docWidth, uint32_t docHeight,
                std::string layerName = "Layer 1");

    uint32_t width()  const noexcept { return _width; }
    uint32_t height() const noexcept { return _height; }

    TileManager&       tiles()       noexcept { return _tiles; }
    const TileManager& tiles() const noexcept { return _tiles; }

    // Paint one RGBA8 pixel at (x, y) using src-over compositing.
    void paintPixel(int x, int y,
                    uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept;

    // Composite this layer (at full opacity, Normal blend) into an external
    // RGBA8 buffer of size outW × outH.  Used for QPainter display.
    void composite(uint8_t* out, uint32_t outW, uint32_t outH) const;

    bool anyDirty()   const;
    void clearDirty();

private:
    uint32_t    _width, _height;
    TileManager _tiles;
};

} // namespace dexilate::engine
