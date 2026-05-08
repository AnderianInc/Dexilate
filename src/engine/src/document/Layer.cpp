// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/engine/document/Layer.h"

#include <cstring>

namespace dexilate::engine {

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
    // Fill output with transparent black first
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

} // namespace dexilate::engine
