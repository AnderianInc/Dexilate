// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/engine/viewport/ViewTransform.h"

#include <algorithm>

namespace dexilate::engine {

void ViewTransform::zoomAround(glm::vec2 anchor, float factor) {
    float newZoom = std::clamp(_zoom * factor, 0.05f, 64.0f);
    // Keep anchor fixed: pan must compensate for the zoom change.
    _pan   = anchor + ((_pan - anchor) * (newZoom / _zoom));
    _zoom  = newZoom;
}

void ViewTransform::fitToViewport(float docW, float docH,
                                   float vpW,  float vpH) {
    float scaleX = vpW / docW;
    float scaleY = vpH / docH;
    _zoom = std::min(scaleX, scaleY);
    _pan  = glm::vec2{
        (vpW - docW * _zoom) * 0.5f,
        (vpH - docH * _zoom) * 0.5f
    };
}

glm::vec2 ViewTransform::screenToCanvas(glm::vec2 screen) const noexcept {
    return (screen - _pan) / _zoom;
}

glm::vec2 ViewTransform::canvasToScreen(glm::vec2 canvas) const noexcept {
    return canvas * _zoom + _pan;
}

glm::mat4 ViewTransform::shaderMatrix(float vpW, float vpH,
                                       float docW, float docH) const noexcept {
    // Map document rect [0,docW]×[0,docH] in screen space to NDC [-1,1].
    // Screen-space rect of the document:
    //   left   = _pan.x
    //   top    = _pan.y
    //   right  = _pan.x + docW * _zoom
    //   bottom = _pan.y + docH * _zoom
    // We build an ortho projection: screen → NDC.
    return glm::ortho(0.0f, vpW, vpH, 0.0f, -1.0f, 1.0f)
         * glm::translate(glm::mat4(1.0f), glm::vec3(_pan, 0.0f))
         * glm::scale(glm::mat4(1.0f), glm::vec3(_zoom * docW, _zoom * docH, 1.0f));
}

} // namespace dexilate::engine
