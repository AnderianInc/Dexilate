// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "dexilate/engine/raster/BrushEngine.h"

namespace dexilate::engine {

// Eraser paints onto the layer alpha channel (erase-to-transparency).
// Uses the same dab geometry as BrushEngine but clears alpha instead of
// compositing colour.
class Eraser {
public:
    void eraseDab(RasterLayer& layer,
                  const BrushDab& dab,
                  const BrushSettings& settings) const;

    float eraseSegment(RasterLayer& layer,
                       const BrushDab& prev,
                       const BrushDab& curr,
                       const BrushSettings& settings,
                       float residualDist = 0.0f) const;
};

} // namespace dexilate::engine
