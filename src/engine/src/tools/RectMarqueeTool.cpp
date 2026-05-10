// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/engine/tools/RectMarqueeTool.h"

#include <algorithm>
#include <cmath>

namespace dexilate::engine {

void RectMarqueeTool::onPress(float x, float y, float /*pressure*/)
{
    _startX = x;
    _startY = y;
    _active = true;
}

void RectMarqueeTool::onMove(float /*x*/, float /*y*/, float /*pressure*/)
{
    // The live selection rect could be visualised here (Phase 3).
    // No persistent state change is required during the drag.
}

void RectMarqueeTool::onRelease(float x, float y)
{
    if (!_active) return;
    _active = false;

    if (!_selection) return;

    // Normalise so that (x0,y0) is always the top-left corner.
    float x0 = std::min(_startX, x);
    float y0 = std::min(_startY, y);
    float x1 = std::max(_startX, x);
    float y1 = std::max(_startY, y);

    int ix = static_cast<int>(std::floor(x0));
    int iy = static_cast<int>(std::floor(y0));
    int iw = static_cast<int>(std::ceil(x1)) - ix;
    int ih = static_cast<int>(std::ceil(y1)) - iy;

    _selection->selectRect(ix, iy, iw, ih);
}

} // namespace dexilate::engine
