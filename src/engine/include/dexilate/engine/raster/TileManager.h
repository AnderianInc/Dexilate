// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "dexilate/core/Tile.h"

#include <memory>
#include <vector>
#include <cstdint>

namespace dexilate::engine {

// Manages the 2-D grid of Tiles that backs a single raster layer.
// The grid is large enough to cover (docWidth × docHeight) pixels;
// the right/bottom partial columns/rows are still full 256×256 tiles
// — the caller clips to document bounds.
class TileManager {
public:
    TileManager(uint32_t docWidth, uint32_t docHeight);

    int tilesX()     const noexcept { return _tilesX; }
    int tilesY()     const noexcept { return _tilesY; }
    int tileCount()  const noexcept { return _tilesX * _tilesY; }

    core::Tile*       at(int tx, int ty)       noexcept;
    const core::Tile* at(int tx, int ty) const noexcept;

    // Tile grid coords that contain document pixel (px, py).
    int tileX(int px) const noexcept { return px / core::Tile::SIZE; }
    int tileY(int py) const noexcept { return py / core::Tile::SIZE; }

    // Linear indices of all tiles with isDirty() == true.
    std::vector<int> dirtyIndices() const;
    void clearAllDirty();

    // Fill every tile with a solid RGBA colour.
    void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

private:
    int _tilesX, _tilesY;
    std::vector<std::unique_ptr<core::Tile>> _tiles;

    int index(int tx, int ty) const noexcept { return ty * _tilesX + tx; }
};

} // namespace dexilate::engine
