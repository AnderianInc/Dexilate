// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace dexilate::engine {

class RasterLayer;

struct BrushSettings {
    glm::vec4 color       = {0.0f, 0.0f, 0.0f, 1.0f};  // RGBA [0, 1]
    float     baseSize    = 20.0f;   // diameter in document pixels at pressure = 1
    float     baseOpacity = 1.0f;    // [0, 1] at full pressure
    float     hardness    = 0.8f;    // [0 = fully soft, 1 = hard edge]
    float     spacing     = 0.25f;   // dab interval as fraction of diameter
};

struct BrushDab {
    float x, y;         // document-space position
    float pressure;     // [0, 1]
    float tiltX = 0.0f; // radians, x-axis
    float tiltY = 0.0f; // radians, y-axis
};

// Stateless CPU raster brush. All painting is src-over into the layer pixels.
class BrushEngine {
public:
    // Paint a single circular dab.
    void paintDab(RasterLayer& layer,
                  const BrushDab& dab,
                  const BrushSettings& settings) const;

    // Interpolate evenly-spaced dabs along a segment from prev→curr.
    // residualDist: leftover fractional distance from the previous call.
    // Returns the new residual for the next call.
    float paintSegment(RasterLayer& layer,
                       const BrushDab& prev,
                       const BrushDab& curr,
                       const BrushSettings& settings,
                       float residualDist = 0.0f) const;
};

} // namespace dexilate::engine
