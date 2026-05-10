// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/engine/tools/LassoTool.h"

namespace dexilate::engine {

void LassoTool::onPress(float x, float y, float /*pressure*/)
{
    _points.clear();
    _points.emplace_back(x, y);
}

void LassoTool::onMove(float x, float y, float /*pressure*/)
{
    _points.emplace_back(x, y);
}

void LassoTool::onRelease(float x, float y)
{
    _points.emplace_back(x, y);

    // Phase 3 will rasterize the lasso polygon.  For now, select everything
    // as a placeholder so that callers can at least verify point collection.
    if (_selection)
        _selection->selectAll();
}

} // namespace dexilate::engine
