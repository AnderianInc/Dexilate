// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/core/Tile.h"

namespace dexilate::core {

void Tile::fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept {
    for (int i = 0; i < SIZE * SIZE; ++i) {
        uint8_t* p = pixels.data() + i * 4;
        p[0] = r; p[1] = g; p[2] = b; p[3] = a;
    }
    _dirty = true;
}

} // namespace dexilate::core
