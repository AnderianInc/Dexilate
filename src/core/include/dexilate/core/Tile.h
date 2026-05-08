// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include <array>
#include <cstdint>

namespace dexilate::core {

// 256×256 RGBA8 pixel buffer — the smallest unit of raster storage.
// Pixels are stored row-major, RGBA channel order, 8 bits per channel, sRGB.
class Tile {
public:
    static constexpr int SIZE  = 256;
    static constexpr int BYTES = SIZE * SIZE * 4;  // 262 144 bytes per tile

    // Fill the entire tile with a solid color (channels in [0, 255]).
    void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept;

    // Pixel access — (x, y) must be in [0, SIZE).
    uint8_t* pixelAt(int x, int y) noexcept {
        return pixels.data() + (y * SIZE + x) * 4;
    }
    const uint8_t* pixelAt(int x, int y) const noexcept {
        return pixels.data() + (y * SIZE + x) * 4;
    }

    uint8_t*       data()       noexcept { return pixels.data(); }
    const uint8_t* data() const noexcept { return pixels.data(); }

    bool isDirty()  const noexcept { return _dirty; }
    void markDirty()      noexcept { _dirty = true; }
    void clearDirty()     noexcept { _dirty = false; }

private:
    std::array<uint8_t, BYTES> pixels{};  // zero-initialised → transparent black
    bool _dirty = true;
};

} // namespace dexilate::core
