// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "ITool.h"
#include "dexilate/engine/raster/Selection.h"

namespace dexilate::engine {

// Rectangular marquee selection tool.
//
// Usage:
//   tool.setSelection(&sel);
//   tool.onPress(x, y, pressure);   // anchor corner
//   tool.onMove(x, y, pressure);    // drag to update
//   tool.onRelease(x, y);           // commit rect to Selection
class RectMarqueeTool : public ITool {
public:
    RectMarqueeTool() = default;

    // Associate a Selection that will receive the final rect on onRelease().
    void       setSelection(Selection* sel) noexcept { _selection = sel; }
    Selection* selection() const noexcept            { return _selection; }

    // ITool interface
    void onPress(float x, float y, float pressure) override;
    void onMove(float x, float y, float pressure)  override;
    void onRelease(float x, float y)               override;

private:
    Selection* _selection = nullptr;

    float _startX = 0.0f;
    float _startY = 0.0f;
    bool  _active = false;
};

} // namespace dexilate::engine
