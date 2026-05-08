// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/engine/raster/InputSampler.h"
#include "dexilate/engine/document/Layer.h"

namespace dexilate::engine {

void InputSampler::beginStroke(RasterLayer& layer,
                               float x, float y, float pressure,
                               const BrushEngine& brush,
                               const BrushSettings& settings) {
    _painting     = true;
    _lastX        = x;
    _lastY        = y;
    _lastPressure = pressure;
    _residual     = 0.0f;

    BrushDab dab{x, y, pressure};
    brush.paintDab(layer, dab, settings);
}

void InputSampler::continueStroke(RasterLayer& layer,
                                  float x, float y, float pressure,
                                  const BrushEngine& brush,
                                  const BrushSettings& settings) {
    if (!_painting) return;

    BrushDab prev{_lastX, _lastY, _lastPressure};
    BrushDab curr{x, y, pressure};
    _residual = brush.paintSegment(layer, prev, curr, settings, _residual);

    _lastX        = x;
    _lastY        = y;
    _lastPressure = pressure;
}

void InputSampler::endStroke() {
    _painting = false;
    _residual = 0.0f;
}

} // namespace dexilate::engine
