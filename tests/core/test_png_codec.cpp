// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  tests/core/test_png_codec.cpp
//  Round-trip tests for PngCodec — headless, no display required.
// ─────────────────────────────────────────────────────────────────────────────

#include <catch2/catch_test_macros.hpp>
#include "dexilate/core/codecs/PngCodec.h"

#include <filesystem>
#include <vector>
#include <cstdint>

namespace fs = std::filesystem;
using namespace dexilate::core;

static fs::path tmpPng() {
    return fs::temp_directory_path() / "dexilate_test_png_codec.png";
}

TEST_CASE("PngCodec - encode then decode round-trip", "[codec][png]") {
    constexpr uint32_t W = 64, H = 32;
    std::vector<uint8_t> src(W * H * 4);

    // Fill with a gradient so we're testing real pixel values
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            uint8_t* p = src.data() + (y * W + x) * 4;
            p[0] = static_cast<uint8_t>(x * 4);       // R
            p[1] = static_cast<uint8_t>(y * 8);       // G
            p[2] = 128;                                 // B
            p[3] = 255;                                 // A
        }
    }

    PngCodec codec;
    auto path = tmpPng();

    SECTION("encode does not throw") {
        REQUIRE_NOTHROW(codec.encode(path, W, H, src.data()));
        CHECK(fs::exists(path));
    }

    SECTION("round-trip preserves dimensions and pixels") {
        codec.encode(path, W, H, src.data());
        ImageData img = codec.decode(path);

        CHECK(img.width  == W);
        CHECK(img.height == H);
        REQUIRE(img.pixels.size() == W * H * 4);

        // Pixel-exact round-trip (PNG is lossless)
        for (size_t i = 0; i < src.size(); ++i) {
            REQUIRE(img.pixels[i] == src[i]);
        }
    }

    fs::remove(path);  // clean up
}

TEST_CASE("PngCodec - decode non-existent file throws CodecError", "[codec][png]") {
    PngCodec codec;
    CHECK_THROWS_AS(
        codec.decode(fs::temp_directory_path() / "does_not_exist_xyz.png"),
        CodecError);
}

TEST_CASE("PngCodec - encode then decode single-pixel", "[codec][png]") {
    uint8_t pixel[4] = {255, 0, 128, 200};
    PngCodec codec;
    auto path = tmpPng();
    codec.encode(path, 1, 1, pixel);
    auto img = codec.decode(path);
    CHECK(img.width  == 1);
    CHECK(img.height == 1);
    CHECK(img.pixels[0] == 255);
    CHECK(img.pixels[1] == 0);
    CHECK(img.pixels[2] == 128);
    CHECK(img.pixels[3] == 200);
    fs::remove(path);
}

TEST_CASE("PngCodec - encode then decode fully transparent image", "[codec][png]") {
    constexpr uint32_t W = 8, H = 8;
    std::vector<uint8_t> src(W * H * 4, 0);  // all transparent black
    PngCodec codec;
    auto path = tmpPng();
    codec.encode(path, W, H, src.data());
    auto img = codec.decode(path);
    REQUIRE(img.pixels.size() == W * H * 4);
    for (auto b : img.pixels) CHECK(b == 0);
    fs::remove(path);
}
