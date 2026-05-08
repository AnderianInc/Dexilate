// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  tests/engine/test_brush.cpp
//  Unit tests for BrushEngine, TileManager, RasterLayer — headless.
// ─────────────────────────────────────────────────────────────────────────────

#include <catch2/catch_test_macros.hpp>
#include "dexilate/engine/raster/BrushEngine.h"
#include "dexilate/engine/raster/Eraser.h"
#include "dexilate/engine/document/Layer.h"
#include "dexilate/engine/raster/TileManager.h"

#include <cmath>

using namespace dexilate::engine;

// ── TileManager ───────────────────────────────────────────────────────────────

TEST_CASE("TileManager - grid dimensions for exact multiple", "[tilemanager]") {
    TileManager tm(512, 256);
    CHECK(tm.tilesX() == 2);
    CHECK(tm.tilesY() == 1);
    CHECK(tm.tileCount() == 2);
}

TEST_CASE("TileManager - grid dimensions for non-multiple (ceiling)", "[tilemanager]") {
    TileManager tm(300, 300);  // 300/256 = 1.17… → 2 tiles each axis
    CHECK(tm.tilesX() == 2);
    CHECK(tm.tilesY() == 2);
}

TEST_CASE("TileManager - at() returns non-null for valid indices", "[tilemanager]") {
    TileManager tm(256, 256);
    CHECK(tm.at(0, 0) != nullptr);
    CHECK(tm.at(0, 0) == tm.at(0, 0));  // same pointer
}

TEST_CASE("TileManager - at() returns null for out-of-range", "[tilemanager]") {
    TileManager tm(256, 256);
    CHECK(tm.at(-1,  0) == nullptr);
    CHECK(tm.at( 0, -1) == nullptr);
    CHECK(tm.at( 1,  0) == nullptr);
    CHECK(tm.at( 0,  1) == nullptr);
}

TEST_CASE("TileManager - dirty tracking", "[tilemanager]") {
    TileManager tm(512, 256);
    // All tiles start dirty (new construction)
    CHECK(tm.dirtyIndices().size() == 2);

    tm.clearAllDirty();
    CHECK(tm.dirtyIndices().empty());

    tm.at(1, 0)->markDirty();
    auto dirty = tm.dirtyIndices();
    REQUIRE(dirty.size() == 1);
    CHECK(dirty[0] == 1);  // index 1 = tx=1, ty=0
}

// ── RasterLayer painting ──────────────────────────────────────────────────────

TEST_CASE("RasterLayer - paintPixel in bounds modifies pixel", "[layer]") {
    RasterLayer layer(256, 256);
    layer.paintPixel(0, 0, 255, 0, 0, 255);  // opaque red at (0,0)

    auto* tile = layer.tiles().at(0, 0);
    REQUIRE(tile != nullptr);
    const uint8_t* p = tile->pixelAt(0, 0);
    CHECK(p[0] == 255);  // R
    CHECK(p[1] == 0);    // G
    CHECK(p[2] == 0);    // B
    CHECK(p[3] == 255);  // A
}

TEST_CASE("RasterLayer - paintPixel out of bounds does nothing", "[layer]") {
    RasterLayer layer(256, 256);
    // Should not crash; no tile is modified
    layer.paintPixel(-1, 0, 255, 0, 0, 255);
    layer.paintPixel(0, -1, 255, 0, 0, 255);
    layer.paintPixel(256, 0, 255, 0, 0, 255);
    layer.paintPixel(0, 256, 255, 0, 0, 255);
    // All tiles should still be transparent
    const uint8_t* p = layer.tiles().at(0, 0)->pixelAt(0, 0);
    CHECK(p[3] == 0);
}

TEST_CASE("RasterLayer - paintPixel src-over compositing", "[layer]") {
    RasterLayer layer(256, 256);
    // Paint 50% opaque white on transparent black
    layer.paintPixel(10, 10, 255, 255, 255, 128);  // ~50% white

    auto* tile = layer.tiles().at(0, 0);
    const uint8_t* p = tile->pixelAt(10, 10);
    // Result should be non-zero alpha and white-ish colour
    CHECK(p[3] > 0);
    CHECK(p[0] > 100);  // should be close to 255
}

TEST_CASE("RasterLayer - composite fills output buffer", "[layer]") {
    RasterLayer layer(64, 64);
    // Paint every pixel bright green
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            layer.paintPixel(x, y, 0, 255, 0, 255);

    std::vector<uint8_t> buf(64 * 64 * 4, 0);
    layer.composite(buf.data(), 64, 64);

    for (int i = 0; i < 64 * 64; ++i) {
        CHECK(buf[i * 4 + 0] == 0);    // R
        CHECK(buf[i * 4 + 1] == 255);  // G
        CHECK(buf[i * 4 + 2] == 0);    // B
        CHECK(buf[i * 4 + 3] == 255);  // A
    }
}

// ── BrushEngine ───────────────────────────────────────────────────────────────

TEST_CASE("BrushEngine - paintDab deposits pixels near centre", "[brush]") {
    RasterLayer layer(256, 256);
    BrushEngine  brush;
    BrushSettings settings;
    settings.baseSize    = 20.0f;
    settings.baseOpacity = 1.0f;
    settings.hardness    = 1.0f;
    settings.color       = {0.0f, 0.0f, 1.0f, 1.0f};  // blue

    BrushDab dab{128.0f, 128.0f, 1.0f};
    brush.paintDab(layer, dab, settings);

    auto* tile = layer.tiles().at(0, 0);
    REQUIRE(tile != nullptr);
    const uint8_t* centre = tile->pixelAt(128, 128);
    CHECK(centre[3] > 200);  // centre pixel should be mostly opaque
    CHECK(centre[2] > 200);  // blue channel should be strong
}

TEST_CASE("BrushEngine - zero pressure paints nothing", "[brush]") {
    RasterLayer layer(256, 256);
    BrushEngine  brush;
    BrushSettings settings;
    settings.baseSize = 20.0f;

    BrushDab dab{128.0f, 128.0f, 0.0f};  // zero pressure
    brush.paintDab(layer, dab, settings);

    // No pixel in the dab region should be modified (radius < 0.5 → early return)
    const uint8_t* p = layer.tiles().at(0, 0)->pixelAt(128, 128);
    CHECK(p[3] == 0);
}

TEST_CASE("BrushEngine - paintSegment places multiple dabs", "[brush]") {
    RasterLayer layer(512, 256);
    BrushEngine  brush;
    BrushSettings settings;
    settings.baseSize    = 10.0f;
    settings.baseOpacity = 1.0f;
    settings.hardness    = 1.0f;
    settings.spacing     = 0.5f;
    settings.color       = {1.0f, 0.0f, 0.0f, 1.0f};  // red

    BrushDab start{10.0f,  128.0f, 1.0f};
    BrushDab end  {250.0f, 128.0f, 1.0f};

    float residual = brush.paintSegment(layer, start, end, settings);
    CHECK(residual >= 0.0f);

    // Pixels along y=128 from x=10..250 should have been touched
    int paintedCount = 0;
    for (int x = 10; x < 250; x += 20) {
        int tx = layer.tiles().tileX(x);
        int ty = layer.tiles().tileY(128);
        const uint8_t* p = layer.tiles().at(tx, ty)->pixelAt(x % 256, 128 % 256);
        if (p[3] > 0) ++paintedCount;
    }
    CHECK(paintedCount > 3);  // at least a few dabs landed
}

TEST_CASE("Eraser - eraseDab reduces alpha", "[eraser]") {
    RasterLayer layer(256, 256);

    // First paint a solid opaque red area
    for (int y = 100; y < 156; ++y)
        for (int x = 100; x < 156; ++x)
            layer.paintPixel(x, y, 255, 0, 0, 255);

    const uint8_t* before = layer.tiles().at(0, 0)->pixelAt(128, 128);
    CHECK(before[3] == 255);

    // Erase the centre
    Eraser eraser;
    BrushSettings settings;
    settings.baseSize    = 20.0f;
    settings.baseOpacity = 1.0f;
    settings.hardness    = 1.0f;

    BrushDab dab{128.0f, 128.0f, 1.0f};
    eraser.eraseDab(layer, dab, settings);

    const uint8_t* after = layer.tiles().at(0, 0)->pixelAt(128, 128);
    CHECK(after[3] < 255);  // alpha should have been reduced
}
