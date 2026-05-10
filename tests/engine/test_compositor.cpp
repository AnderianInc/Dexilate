// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  tests/engine/test_compositor.cpp
//  Unit tests for Compositor blend modes, opacity, and adjustment layers.
// ─────────────────────────────────────────────────────────────────────────────

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "dexilate/engine/raster/Compositor.h"
#include "dexilate/engine/document/Document.h"
#include "dexilate/engine/document/Layer.h"

#include <array>
#include <cstdint>
#include <vector>

using namespace dexilate::engine;

// Helper: build a 1×1 Document with a single fully-painted raster layer.
// Returns {doc, layerIndex}.
static Document makeSolidDoc(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255,
                              BlendMode mode = BlendMode::Normal,
                              float opacity = 1.0f)
{
    Document doc(1, 1, "test");
    int idx = doc.addRasterLayer("src");
    auto* layer = dynamic_cast<RasterLayer*>(doc.layerAt(idx));
    layer->paintPixel(0, 0, r, g, b, a);
    layer->blendMode = mode;
    layer->opacity   = opacity;
    return doc;
}

// Composite a 1×1 document and return the single output RGBA pixel.
static std::array<uint8_t, 4> composite1x1(const Document& doc)
{
    std::array<uint8_t, 4> out{};
    Compositor::composite(doc, out.data(), 1, 1);
    return out;
}

// ── Empty layer list ──────────────────────────────────────────────────────────

TEST_CASE("Compositor - empty document produces transparent black", "[compositor]") {
    Document doc(4, 4, "empty");
    std::vector<uint8_t> buf(4 * 4 * 4, 0xAB);   // prefill with junk
    Compositor::composite(doc, buf.data(), 4, 4);

    // Every pixel must be (0,0,0,0).
    for (size_t i = 0; i < buf.size(); ++i)
        REQUIRE(buf[i] == 0);
}

// ── Normal blend ──────────────────────────────────────────────────────────────

TEST_CASE("Compositor - Normal blend: opaque top covers background", "[compositor]") {
    // Background layer: opaque white
    Document doc(1, 1, "normal");
    int bgIdx = doc.addRasterLayer("bg");
    auto* bg = dynamic_cast<RasterLayer*>(doc.layerAt(bgIdx));
    bg->paintPixel(0, 0, 255, 255, 255, 255);

    // Top layer: opaque red (Normal)
    int topIdx = doc.addRasterLayer("top");
    auto* top = dynamic_cast<RasterLayer*>(doc.layerAt(topIdx));
    top->paintPixel(0, 0, 200, 0, 0, 255);
    top->blendMode = BlendMode::Normal;
    top->opacity   = 1.0f;

    auto px = composite1x1(doc);
    REQUIRE(px[0] == 200);
    REQUIRE(px[1] == 0);
    REQUIRE(px[2] == 0);
    REQUIRE(px[3] == 255);
}

// ── Multiply blend ────────────────────────────────────────────────────────────

TEST_CASE("Compositor - Multiply blend: gray over white yields approximately gray", "[compositor]") {
    // Background: opaque white
    Document doc(1, 1, "multiply");
    int bgIdx = doc.addRasterLayer("bg");
    auto* bg = dynamic_cast<RasterLayer*>(doc.layerAt(bgIdx));
    bg->paintPixel(0, 0, 255, 255, 255, 255);

    // Top layer: mid-gray with Multiply
    int topIdx = doc.addRasterLayer("top");
    auto* top = dynamic_cast<RasterLayer*>(doc.layerAt(topIdx));
    top->paintPixel(0, 0, 128, 128, 128, 255);
    top->blendMode = BlendMode::Multiply;
    top->opacity   = 1.0f;

    auto px = composite1x1(doc);
    // Multiply(128, 255) = 128*255/255 = 128; allow ±2 rounding
    REQUIRE(px[3] == 255);
    REQUIRE_THAT(static_cast<float>(px[0]), Catch::Matchers::WithinAbs(128.0f, 2.0f));
    REQUIRE_THAT(static_cast<float>(px[1]), Catch::Matchers::WithinAbs(128.0f, 2.0f));
    REQUIRE_THAT(static_cast<float>(px[2]), Catch::Matchers::WithinAbs(128.0f, 2.0f));
}

// ── Screen blend ──────────────────────────────────────────────────────────────

TEST_CASE("Compositor - Screen blend is brighter than Multiply for same values", "[compositor]") {
    auto makeDoc = [](BlendMode mode) {
        Document doc(1, 1, "blend");
        int bgIdx = doc.addRasterLayer("bg");
        auto* bg = dynamic_cast<RasterLayer*>(doc.layerAt(bgIdx));
        bg->paintPixel(0, 0, 128, 128, 128, 255);

        int topIdx = doc.addRasterLayer("top");
        auto* top = dynamic_cast<RasterLayer*>(doc.layerAt(topIdx));
        top->paintPixel(0, 0, 128, 128, 128, 255);
        top->blendMode = mode;
        top->opacity   = 1.0f;
        return doc;
    };

    Document mulDoc    = makeDoc(BlendMode::Multiply);
    Document screenDoc = makeDoc(BlendMode::Screen);

    std::array<uint8_t, 4> mulPx, screenPx;
    Compositor::composite(mulDoc,    mulPx.data(),    1, 1);
    Compositor::composite(screenDoc, screenPx.data(), 1, 1);

    // Screen must produce a lighter result than Multiply on the R channel.
    REQUIRE(screenPx[0] > mulPx[0]);
}

// ── Transparency / opacity ────────────────────────────────────────────────────

TEST_CASE("Compositor - 50% opacity layer over white gives midpoint", "[compositor]") {
    // Background: opaque white
    Document doc(1, 1, "opacity");
    int bgIdx = doc.addRasterLayer("bg");
    auto* bg = dynamic_cast<RasterLayer*>(doc.layerAt(bgIdx));
    bg->paintPixel(0, 0, 255, 255, 255, 255);

    // Top: opaque black with 0.5 opacity
    int topIdx = doc.addRasterLayer("top");
    auto* top = dynamic_cast<RasterLayer*>(doc.layerAt(topIdx));
    top->paintPixel(0, 0, 0, 0, 0, 255);
    top->blendMode = BlendMode::Normal;
    top->opacity   = 0.5f;

    auto px = composite1x1(doc);
    REQUIRE(px[3] == 255);
    // Each channel should be close to 127/128.
    REQUIRE_THAT(static_cast<float>(px[0]), Catch::Matchers::WithinAbs(128.0f, 2.0f));
    REQUIRE_THAT(static_cast<float>(px[1]), Catch::Matchers::WithinAbs(128.0f, 2.0f));
    REQUIRE_THAT(static_cast<float>(px[2]), Catch::Matchers::WithinAbs(128.0f, 2.0f));
}

TEST_CASE("Compositor - fully transparent layer changes nothing", "[compositor]") {
    Document doc(1, 1, "transparent");
    int bgIdx = doc.addRasterLayer("bg");
    auto* bg = dynamic_cast<RasterLayer*>(doc.layerAt(bgIdx));
    bg->paintPixel(0, 0, 100, 150, 200, 255);

    int topIdx = doc.addRasterLayer("top");
    auto* top = dynamic_cast<RasterLayer*>(doc.layerAt(topIdx));
    top->paintPixel(0, 0, 255, 0, 0, 255);
    top->opacity = 0.0f;   // fully transparent

    auto px = composite1x1(doc);
    REQUIRE(px[0] == 100);
    REQUIRE(px[1] == 150);
    REQUIRE(px[2] == 200);
    REQUIRE(px[3] == 255);
}

// ── BrightnessContrastLayer ───────────────────────────────────────────────────

TEST_CASE("BrightnessContrastLayer - positive brightness raises channel values", "[compositor][adjustment]") {
    // Build a single grey pixel buffer.
    constexpr uint32_t W = 2, H = 2;
    std::vector<uint8_t> buf(W * H * 4, 0);
    for (size_t i = 0; i < W * H; ++i) {
        buf[i * 4 + 0] = 100;
        buf[i * 4 + 1] = 100;
        buf[i * 4 + 2] = 100;
        buf[i * 4 + 3] = 255;
    }

    BrightnessContrastLayer adj;
    adj.brightness = 0.5f;  // +50%
    adj.contrast   = 0.0f;
    adj.apply(buf.data(), W, H);

    // All channel values should have increased.
    for (size_t i = 0; i < W * H; ++i) {
        REQUIRE(buf[i * 4 + 0] > 100);
        REQUIRE(buf[i * 4 + 1] > 100);
        REQUIRE(buf[i * 4 + 2] > 100);
        REQUIRE(buf[i * 4 + 3] == 255);  // alpha untouched
    }
}

TEST_CASE("BrightnessContrastLayer - negative brightness lowers channel values", "[compositor][adjustment]") {
    constexpr uint32_t W = 1, H = 1;
    std::vector<uint8_t> buf(4, 0);
    buf[0] = 200; buf[1] = 200; buf[2] = 200; buf[3] = 255;

    BrightnessContrastLayer adj;
    adj.brightness = -0.5f;
    adj.contrast   = 0.0f;
    adj.apply(buf.data(), W, H);

    REQUIRE(buf[0] < 200);
    REQUIRE(buf[1] < 200);
    REQUIRE(buf[2] < 200);
    REQUIRE(buf[3] == 255);
}

TEST_CASE("BrightnessContrastLayer - zero brightness/contrast is identity", "[compositor][adjustment]") {
    constexpr uint32_t W = 1, H = 1;
    std::vector<uint8_t> buf = {80, 120, 200, 255};

    BrightnessContrastLayer adj;
    adj.brightness = 0.0f;
    adj.contrast   = 0.0f;
    adj.apply(buf.data(), W, H);

    REQUIRE(buf[0] == 80);
    REQUIRE(buf[1] == 120);
    REQUIRE(buf[2] == 200);
    REQUIRE(buf[3] == 255);
}

TEST_CASE("BrightnessContrastLayer - applied via Document compositor affects output", "[compositor][adjustment]") {
    Document doc(1, 1, "adj-doc");

    // Background: mid-gray
    int bgIdx = doc.addRasterLayer("bg");
    auto* bg = dynamic_cast<RasterLayer*>(doc.layerAt(bgIdx));
    bg->paintPixel(0, 0, 100, 100, 100, 255);

    // Brightness adjustment on top
    auto adjLayer = std::make_unique<BrightnessContrastLayer>();
    adjLayer->brightness = 0.5f;
    adjLayer->contrast   = 0.0f;
    doc.addLayer(std::move(adjLayer));

    auto px = composite1x1(doc);
    REQUIRE(px[0] > 100);
    REQUIRE(px[3] == 255);
}
