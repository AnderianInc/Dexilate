// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  tests/core/test_tile.cpp
//  Unit tests for dexilate::core::Tile — headless, no display required.
// ─────────────────────────────────────────────────────────────────────────────

#include <catch2/catch_test_macros.hpp>
#include "dexilate/core/Tile.h"

using namespace dexilate::core;

TEST_CASE("Tile - constants", "[tile]") {
    CHECK(Tile::SIZE  == 256);
    CHECK(Tile::BYTES == 256 * 256 * 4);
}

TEST_CASE("Tile - zero initialised on construction", "[tile]") {
    Tile t;
    const uint8_t* p = t.pixelAt(0, 0);
    CHECK(p[0] == 0);
    CHECK(p[1] == 0);
    CHECK(p[2] == 0);
    CHECK(p[3] == 0);
    CHECK(p == t.data());
}

TEST_CASE("Tile - fill sets all pixels", "[tile]") {
    Tile t;
    t.fill(255, 128, 64, 200);

    for (int y = 0; y < Tile::SIZE; ++y) {
        for (int x = 0; x < Tile::SIZE; ++x) {
            const uint8_t* p = t.pixelAt(x, y);
            REQUIRE(p[0] == 255);
            REQUIRE(p[1] == 128);
            REQUIRE(p[2] == 64);
            REQUIRE(p[3] == 200);
        }
    }
}

TEST_CASE("Tile - pixelAt corner addresses are distinct", "[tile]") {
    Tile t;
    const uint8_t* tl = t.pixelAt(0, 0);
    const uint8_t* tr = t.pixelAt(Tile::SIZE - 1, 0);
    const uint8_t* bl = t.pixelAt(0, Tile::SIZE - 1);
    const uint8_t* br = t.pixelAt(Tile::SIZE - 1, Tile::SIZE - 1);

    CHECK(tl != tr);
    CHECK(tl != bl);
    CHECK(tl != br);
    CHECK(tr != bl);
    CHECK(tr != br);
    CHECK(bl != br);
}

TEST_CASE("Tile - pixelAt stride is 4 bytes per pixel", "[tile]") {
    Tile t;
    CHECK(t.pixelAt(1, 0) == t.pixelAt(0, 0) + 4);
    CHECK(t.pixelAt(0, 1) == t.pixelAt(0, 0) + Tile::SIZE * 4);
}

TEST_CASE("Tile - dirty flag", "[tile]") {
    Tile t;
    // New tiles start dirty (needs initial GPU upload)
    CHECK(t.isDirty());
    t.clearDirty();
    CHECK_FALSE(t.isDirty());
    t.markDirty();
    CHECK(t.isDirty());
}

TEST_CASE("Tile - fill marks dirty", "[tile]") {
    Tile t;
    t.clearDirty();
    CHECK_FALSE(t.isDirty());
    t.fill(0, 0, 0);
    CHECK(t.isDirty());
}

TEST_CASE("Tile - write via pixelAt and read back", "[tile]") {
    Tile t;
    uint8_t* p = t.pixelAt(100, 150);
    p[0] = 10; p[1] = 20; p[2] = 30; p[3] = 40;

    const uint8_t* q = t.pixelAt(100, 150);
    CHECK(q[0] == 10);
    CHECK(q[1] == 20);
    CHECK(q[2] == 30);
    CHECK(q[3] == 40);

    // Neighbours should be unaffected
    CHECK(t.pixelAt(101, 150)[0] == 0);
    CHECK(t.pixelAt(100, 151)[0] == 0);
}
