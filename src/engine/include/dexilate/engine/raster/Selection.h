// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include <vector>
#include <cstdint>

namespace dexilate::engine {

// 1-byte per pixel selection mask at document resolution.
// 0 = not selected, 255 = fully selected.
class Selection {
public:
    Selection() = default;
    Selection(uint32_t width, uint32_t height);

    uint32_t width()  const noexcept { return _width; }
    uint32_t height() const noexcept { return _height; }
    bool     empty()  const noexcept { return _mask.empty(); }

    bool isSelected(int x, int y) const noexcept;
    void selectAll();
    void deselect();
    void selectRect(int x, int y, int w, int h);
    void invert();

    const std::vector<uint8_t>& mask() const noexcept { return _mask; }

private:
    uint32_t _width  = 0;
    uint32_t _height = 0;
    std::vector<uint8_t> _mask;  // _width * _height bytes
};

} // namespace dexilate::engine
