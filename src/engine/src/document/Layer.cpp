// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/engine/document/Layer.h"

#include <cstring>
#include <algorithm>
#include <cmath>

namespace dexilate::engine {

// ── RasterLayer ────────────────────────────────────────────────────────────────

RasterLayer::RasterLayer(uint32_t docWidth, uint32_t docHeight,
                         std::string layerName)
    : _width(docWidth), _height(docHeight)
    , _tiles(docWidth, docHeight)
{
    name = std::move(layerName);
}

void RasterLayer::paintPixel(int x, int y,
                              uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept {
    if (x < 0 || y < 0 || x >= static_cast<int>(_width) || y >= static_cast<int>(_height))
        return;

    core::Tile* tile = _tiles.at(_tiles.tileX(x), _tiles.tileY(y));
    if (!tile) return;

    int lx = x % core::Tile::SIZE;
    int ly = y % core::Tile::SIZE;
    uint8_t* dst = tile->pixelAt(lx, ly);

    // Porter-Duff src-over (straight alpha)
    float srcA = a / 255.0f;
    float dstA = dst[3] / 255.0f;
    float outA = srcA + dstA * (1.0f - srcA);

    if (outA > 0.0f) {
        float invA = 1.0f / outA;
        auto blend = [&](float src, float d) -> uint8_t {
            float v = (src * srcA + (d / 255.0f) * dstA * (1.0f - srcA)) * invA;
            return static_cast<uint8_t>(std::min(1.0f, v) * 255.0f + 0.5f);
        };
        dst[0] = blend(r / 255.0f, dst[0]);
        dst[1] = blend(g / 255.0f, dst[1]);
        dst[2] = blend(b / 255.0f, dst[2]);
        dst[3] = static_cast<uint8_t>(outA * 255.0f + 0.5f);
    }
    tile->markDirty();
}

void RasterLayer::composite(uint8_t* out, uint32_t outW, uint32_t outH) const {
    std::memset(out, 0, static_cast<size_t>(outW) * outH * 4);

    uint32_t copyW = std::min(outW, _width);
    uint32_t copyH = std::min(outH, _height);

    for (uint32_t y = 0; y < copyH; ++y) {
        for (uint32_t x = 0; x < copyW; ++x) {
            int tx = _tiles.tileX(static_cast<int>(x));
            int ty = _tiles.tileY(static_cast<int>(y));
            const core::Tile* tile = _tiles.at(tx, ty);
            if (!tile) continue;

            int lx = static_cast<int>(x) % core::Tile::SIZE;
            int ly = static_cast<int>(y) % core::Tile::SIZE;
            const uint8_t* src = tile->pixelAt(lx, ly);
            uint8_t*       dst = out + (y * outW + x) * 4;
            dst[0] = src[0]; dst[1] = src[1];
            dst[2] = src[2]; dst[3] = src[3];
        }
    }
}

bool RasterLayer::anyDirty() const {
    return !_tiles.dirtyIndices().empty();
}

void RasterLayer::clearDirty() {
    _tiles.clearAllDirty();
}

// ── GroupLayer ─────────────────────────────────────────────────────────────────

GroupLayer::GroupLayer(std::string layerName) {
    name = std::move(layerName);
}

void GroupLayer::addLayer(std::unique_ptr<Layer> layer) {
    _children.push_back(std::move(layer));
}

void GroupLayer::removeLayer(int index) {
    if (index < 0 || index >= static_cast<int>(_children.size())) return;
    _children.erase(_children.begin() + index);
}

int GroupLayer::layerCount() const noexcept {
    return static_cast<int>(_children.size());
}

Layer* GroupLayer::layerAt(int index) noexcept {
    if (index < 0 || index >= static_cast<int>(_children.size())) return nullptr;
    return _children[static_cast<size_t>(index)].get();
}

const Layer* GroupLayer::layerAt(int index) const noexcept {
    if (index < 0 || index >= static_cast<int>(_children.size())) return nullptr;
    return _children[static_cast<size_t>(index)].get();
}

void GroupLayer::composite(uint8_t* out, uint32_t outW, uint32_t outH) const {
    // Bottom-to-top compositing mirrors the Document::flatten approach.
    // Reuse the Compositor logic by calling per-layer composite directly here
    // to avoid a circular dependency on Compositor.h.
    std::memset(out, 0, static_cast<size_t>(outW) * outH * 4);

    std::vector<uint8_t> layerBuf(static_cast<size_t>(outW) * outH * 4);

    for (const auto& child : _children) {
        if (!child->visible) continue;

        if (auto* raster = dynamic_cast<const RasterLayer*>(child.get())) {
            raster->composite(layerBuf.data(), outW, outH);
        } else if (auto* group = dynamic_cast<const GroupLayer*>(child.get())) {
            group->composite(layerBuf.data(), outW, outH);
        } else {
            continue;
        }

        float opacity = child->opacity;
        size_t total = static_cast<size_t>(outW) * outH;
        for (size_t i = 0; i < total; ++i) {
            const uint8_t* src = layerBuf.data() + i * 4;
            uint8_t*       dst = out + i * 4;

            float srcA = (src[3] / 255.0f) * opacity;
            float dstA = dst[3] / 255.0f;
            float outA = srcA + dstA * (1.0f - srcA);

            if (outA > 0.0f) {
                float inv = 1.0f / outA;
                dst[0] = static_cast<uint8_t>((src[0] * srcA + dst[0] * dstA * (1.0f - srcA)) * inv + 0.5f);
                dst[1] = static_cast<uint8_t>((src[1] * srcA + dst[1] * dstA * (1.0f - srcA)) * inv + 0.5f);
                dst[2] = static_cast<uint8_t>((src[2] * srcA + dst[2] * dstA * (1.0f - srcA)) * inv + 0.5f);
                dst[3] = static_cast<uint8_t>(outA * 255.0f + 0.5f);
            }
        }
    }
}

// ── AdjustmentLayer ────────────────────────────────────────────────────────────

AdjustmentLayer::AdjustmentLayer(AdjustmentType type, std::string layerName)
    : _type(type)
{
    name = std::move(layerName);
}

// ── BrightnessContrastLayer ────────────────────────────────────────────────────

BrightnessContrastLayer::BrightnessContrastLayer()
    : AdjustmentLayer(AdjustmentType::BrightnessContrast, "Brightness/Contrast")
{}

void BrightnessContrastLayer::apply(uint8_t* pixels, uint32_t w, uint32_t h) const {
    // Brightness shifts the midpoint; contrast scales around 0.5.
    // Formula matches Photoshop legacy mode for predictable behaviour.
    size_t total = static_cast<size_t>(w) * h;
    for (size_t i = 0; i < total; ++i) {
        uint8_t* px = pixels + i * 4;
        for (int c = 0; c < 3; ++c) {
            float v = px[c] / 255.0f;
            v += brightness;
            v = (v - 0.5f) * (1.0f + contrast) + 0.5f;
            v = std::max(0.0f, std::min(1.0f, v));
            px[c] = static_cast<uint8_t>(v * 255.0f + 0.5f);
        }
    }
}

} // namespace dexilate::engine
