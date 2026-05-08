// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "dexilate/core/ICodec.h"

namespace dexilate::core {

// PNG read/write via libpng 1.6.
// Decode: any PNG colour type → RGBA8.
// Encode: RGBA8 → 8-bit RGBA PNG (no alpha premultiplication).
class PngCodec final : public ICodec {
public:
    ImageData decode(const std::filesystem::path& path) override;
    void encode(const std::filesystem::path& path,
                uint32_t width, uint32_t height,
                const uint8_t* rgba8) override;
};

} // namespace dexilate::core
