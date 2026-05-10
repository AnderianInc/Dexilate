// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "dexilate/engine/document/Layer.h"

#include <cstdint>
#include <vector>

namespace dexilate::engine {

class Document;

class Compositor {
public:
    // Composite all visible layers of the document into an RGBA8 output buffer.
    static void composite(const Document& doc, uint8_t* out, uint32_t w, uint32_t h);

private:
    // Apply src-over compositing of src onto dst, taking blendMode and opacity into account.
    static void blendPixel(uint8_t* dst, const uint8_t* src,
                           BlendMode mode, float opacity) noexcept;
};

} // namespace dexilate::engine
