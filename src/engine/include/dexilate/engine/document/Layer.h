// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "dexilate/engine/raster/TileManager.h"
#include "dexilate/core/Tile.h"

#include <memory>
#include <string>
#include <vector>
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
    bool        locked    = false;
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

// ── Group layer ────────────────────────────────────────────────────────────────
class GroupLayer : public Layer {
public:
    explicit GroupLayer(std::string name = "Group");

    void addLayer(std::unique_ptr<Layer> layer);
    void removeLayer(int index);
    int  layerCount() const noexcept;
    Layer*       layerAt(int index)       noexcept;
    const Layer* layerAt(int index) const noexcept;

    // Composite all child layers into output buffer using their blend modes.
    void composite(uint8_t* out, uint32_t outW, uint32_t outH) const;

private:
    std::vector<std::unique_ptr<Layer>> _children;
};

// ── Adjustment layers ──────────────────────────────────────────────────────────
enum class AdjustmentType { Curves, Levels, HueSaturation, ColorBalance, BrightnessContrast };

class AdjustmentLayer : public Layer {
public:
    explicit AdjustmentLayer(AdjustmentType type, std::string name = "Adjustment");
    AdjustmentType adjustmentType() const noexcept { return _type; }

    // Apply this adjustment in-place to an RGBA8 buffer.
    virtual void apply(uint8_t* pixels, uint32_t w, uint32_t h) const = 0;

private:
    AdjustmentType _type;
};

class BrightnessContrastLayer : public AdjustmentLayer {
public:
    BrightnessContrastLayer();
    float brightness = 0.0f;  // -1.0 to +1.0
    float contrast   = 0.0f;  // -1.0 to +1.0
    void apply(uint8_t* pixels, uint32_t w, uint32_t h) const override;
};

} // namespace dexilate::engine
