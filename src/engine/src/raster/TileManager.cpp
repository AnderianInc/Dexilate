// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/engine/raster/TileManager.h"

namespace dexilate::engine {

TileManager::TileManager(uint32_t docWidth, uint32_t docHeight) {
    _tilesX = (static_cast<int>(docWidth)  + core::Tile::SIZE - 1) / core::Tile::SIZE;
    _tilesY = (static_cast<int>(docHeight) + core::Tile::SIZE - 1) / core::Tile::SIZE;
    _tiles.resize(static_cast<size_t>(_tilesX) * _tilesY);
    for (auto& t : _tiles)
        t = std::make_unique<core::Tile>();
}

core::Tile* TileManager::at(int tx, int ty) noexcept {
    if (tx < 0 || ty < 0 || tx >= _tilesX || ty >= _tilesY) return nullptr;
    return _tiles[static_cast<size_t>(index(tx, ty))].get();
}

const core::Tile* TileManager::at(int tx, int ty) const noexcept {
    if (tx < 0 || ty < 0 || tx >= _tilesX || ty >= _tilesY) return nullptr;
    return _tiles[static_cast<size_t>(index(tx, ty))].get();
}

std::vector<int> TileManager::dirtyIndices() const {
    std::vector<int> result;
    result.reserve(static_cast<size_t>(_tilesX * _tilesY));
    for (int i = 0; i < _tilesX * _tilesY; ++i)
        if (_tiles[static_cast<size_t>(i)]->isDirty())
            result.push_back(i);
    return result;
}

void TileManager::clearAllDirty() {
    for (auto& t : _tiles)
        t->clearDirty();
}

void TileManager::fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (auto& t : _tiles)
        t->fill(r, g, b, a);
}

} // namespace dexilate::engine
