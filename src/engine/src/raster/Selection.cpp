// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/engine/raster/Selection.h"

#include <cstring>
#include <algorithm>

namespace dexilate::engine {

Selection::Selection(uint32_t width, uint32_t height)
    : _width(width), _height(height)
    , _mask(static_cast<size_t>(width) * height, 0)
{}

bool Selection::isSelected(int x, int y) const noexcept {
    if (x < 0 || y < 0 ||
        x >= static_cast<int>(_width) || y >= static_cast<int>(_height))
        return false;
    return _mask[static_cast<size_t>(y) * _width + static_cast<size_t>(x)] != 0;
}

void Selection::selectAll() {
    if (_mask.empty()) return;
    std::memset(_mask.data(), 0xFF, _mask.size());
}

void Selection::deselect() {
    std::memset(_mask.data(), 0, _mask.size());
}

void Selection::selectRect(int x, int y, int w, int h) {
    if (_mask.empty()) return;

    int x0 = std::max(x, 0);
    int y0 = std::max(y, 0);
    int x1 = std::min(x + w, static_cast<int>(_width));
    int y1 = std::min(y + h, static_cast<int>(_height));

    for (int row = y0; row < y1; ++row) {
        uint8_t* rowPtr = _mask.data() + static_cast<size_t>(row) * _width;
        std::memset(rowPtr + x0, 0xFF, static_cast<size_t>(x1 - x0));
    }
}

void Selection::invert() {
    for (auto& byte : _mask)
        byte = (byte == 0) ? 0xFF : 0;
}

} // namespace dexilate::engine
