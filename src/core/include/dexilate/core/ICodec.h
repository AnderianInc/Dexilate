// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace dexilate::core {

// Raw decoded image — RGBA8, row-major, top-left origin.
struct ImageData {
    uint32_t             width  = 0;
    uint32_t             height = 0;
    std::vector<uint8_t> pixels;  // width * height * 4 bytes
};

class CodecError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Stateless encode/decode interface. One codec per file format.
class ICodec {
public:
    virtual ~ICodec() = default;

    virtual ImageData decode(const std::filesystem::path& path) = 0;

    virtual void encode(const std::filesystem::path& path,
                        uint32_t width, uint32_t height,
                        const uint8_t* rgba8) = 0;
};

} // namespace dexilate::core
