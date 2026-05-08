// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace dexilate::engine {

// 2-D view transform: pan + zoom (rotation deferred to Phase 2).
// All coordinates are in logical pixels (DPR-independent).
//
// Canvas → Screen:  screenPt = pan + canvasPt * zoom
// Screen → Canvas:  canvasPt = (screenPt - pan) / zoom
class ViewTransform {
public:
    // Adjust zoom by a multiplier, keeping the screen point `anchor` fixed.
    void zoomAround(glm::vec2 anchor, float factor);

    // Pan by a screen-space delta.
    void pan(glm::vec2 delta) { _pan += delta; }

    // Fit the document rectangle (0,0,docW,docH) into a viewport of size (vpW,vpH).
    void fitToViewport(float docW, float docH, float vpW, float vpH);

    void  setZoom(float z) { _zoom = z; }
    float zoom()    const noexcept { return _zoom; }

    glm::vec2 panOffset() const noexcept { return _pan; }

    // Convert between coordinate spaces.
    glm::vec2 screenToCanvas(glm::vec2 screen) const noexcept;
    glm::vec2 canvasToScreen(glm::vec2 canvas) const noexcept;

    // 4×4 MVP for the wgpu composite shader (NDC transform for a full-viewport quad).
    glm::mat4 shaderMatrix(float vpW, float vpH,
                            float docW, float docH) const noexcept;

private:
    glm::vec2 _pan  = {0.0f, 0.0f};
    float     _zoom = 1.0f;
};

} // namespace dexilate::engine
