// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "ITool.h"
#include "dexilate/engine/raster/Selection.h"

#include <utility>
#include <vector>

namespace dexilate::engine {

// Freehand lasso selection tool (Phase 2 stub).
//
// Collects pointer positions during a drag gesture.  Lasso rasterization
// (point-in-polygon fill) is implemented in Phase 3; for now onRelease()
// calls Selection::setAll(true) as a placeholder so that tests can verify
// the point collection logic without depending on Phase 3 code.
class LassoTool : public ITool {
public:
    LassoTool() = default;

    void setSelection(Selection* sel) noexcept { _selection = sel; }
    Selection* selection() const noexcept      { return _selection; }

    // Read-only access to the collected points (for unit testing).
    const std::vector<std::pair<float, float>>& points() const noexcept { return _points; }

    // ITool interface
    void onPress(float x, float y, float pressure) override;
    void onMove(float x, float y, float pressure)  override;
    void onRelease(float x, float y)               override;

private:
    Selection* _selection = nullptr;
    std::vector<std::pair<float, float>> _points;
};

} // namespace dexilate::engine
