// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "dexilate/engine/raster/BrushEngine.h"

namespace dexilate::engine {

class RasterLayer;

// Converts a raw pointer/stylus event stream into a sequence of BrushDabs,
// handling dab spacing and residual distance across events.
// Used when the render loop is driven by IWindow (non-Qt path).
class InputSampler {
public:
    // Called when the pointer button is pressed at (x, y) with given pressure.
    void beginStroke(RasterLayer& layer,
                     float x, float y, float pressure,
                     const BrushEngine& brush,
                     const BrushSettings& settings);

    // Called on each pointer move while pressed.
    void continueStroke(RasterLayer& layer,
                        float x, float y, float pressure,
                        const BrushEngine& brush,
                        const BrushSettings& settings);

    // Called when the pointer button is released.
    void endStroke();

    bool isPainting() const noexcept { return _painting; }

private:
    bool     _painting     = false;
    float    _lastX        = 0.0f;
    float    _lastY        = 0.0f;
    float    _lastPressure = 1.0f;
    float    _residual     = 0.0f;
};

} // namespace dexilate::engine
