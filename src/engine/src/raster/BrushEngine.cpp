// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/engine/raster/BrushEngine.h"
#include "dexilate/engine/document/Layer.h"

#include <cmath>
#include <algorithm>

namespace dexilate::engine {

void BrushEngine::paintDab(RasterLayer& layer,
                            const BrushDab& dab,
                            const BrushSettings& settings) const {
    float radius = settings.baseSize * dab.pressure * 0.5f;
    if (radius < 0.5f) return;

    int x0 = std::max(0, static_cast<int>(std::floor(dab.x - radius)));
    int y0 = std::max(0, static_cast<int>(std::floor(dab.y - radius)));
    int x1 = std::min(static_cast<int>(layer.width())  - 1,
                      static_cast<int>(std::ceil(dab.x + radius)));
    int y1 = std::min(static_cast<int>(layer.height()) - 1,
                      static_cast<int>(std::ceil(dab.y + radius)));

    float srcR = settings.color.r;
    float srcG = settings.color.g;
    float srcB = settings.color.b;
    float alphaScale = settings.color.a * settings.baseOpacity * dab.pressure;

    // Clamp hardness so we don't divide by zero
    float h = std::clamp(settings.hardness, 0.0f, 0.9999f);

    for (int py = y0; py <= y1; ++py) {
        for (int px = x0; px <= x1; ++px) {
            float dx = static_cast<float>(px) - dab.x;
            float dy = static_cast<float>(py) - dab.y;
            float r  = std::sqrt(dx * dx + dy * dy) / radius;
            if (r >= 1.0f) continue;

            // Smoothstep falloff from hardness boundary to edge
            float alpha;
            if (r <= h) {
                alpha = 1.0f;
            } else {
                float t = (r - h) / (1.0f - h);
                alpha = 1.0f - t * t * (3.0f - 2.0f * t);  // smoothstep
            }
            alpha *= alphaScale;

            auto u8 = [](float f) -> uint8_t {
                return static_cast<uint8_t>(std::clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
            layer.paintPixel(px, py,
                             u8(srcR), u8(srcG), u8(srcB),
                             u8(alpha));
        }
    }
}

float BrushEngine::paintSegment(RasterLayer& layer,
                                const BrushDab& prev,
                                const BrushDab& curr,
                                const BrushSettings& settings,
                                float residualDist) const {
    float dx = curr.x - prev.x;
    float dy = curr.y - prev.y;
    float segLen = std::sqrt(dx * dx + dy * dy);
    if (segLen < 1e-4f) return residualDist;

    float spacing = std::max(1.0f, settings.baseSize * settings.spacing);
    float t = (spacing - residualDist) / segLen;

    while (t <= 1.0f) {
        BrushDab dab;
        dab.x        = prev.x + dx * t;
        dab.y        = prev.y + dy * t;
        dab.pressure = prev.pressure + (curr.pressure - prev.pressure) * t;
        paintDab(layer, dab, settings);
        t += spacing / segLen;
    }

    // Remaining distance after the last dab
    float travelled = (t - spacing / segLen) * segLen;
    return segLen - travelled + residualDist;
}

} // namespace dexilate::engine
