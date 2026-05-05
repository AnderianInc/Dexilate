// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  tests/platform/test_input_event.cpp
//
//  Unit tests for InputEvent types and the Modifiers/KeyCode enums.
//  These tests are headless — no window or display is required.
// ─────────────────────────────────────────────────────────────────────────────

#include <catch2/catch_test_macros.hpp>
#include "dexilate/platform/InputEvent.h"

#include <variant>
#include <type_traits>

using namespace dexilate::platform;

// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("Modifiers — bitmask operations", "[input][modifiers]") {
    SECTION("None has zero value") {
        CHECK(static_cast<uint32_t>(Modifiers::None) == 0u);
    }
    SECTION("Individual bits do not overlap") {
        auto shift = static_cast<uint32_t>(Modifiers::Shift);
        auto ctrl  = static_cast<uint32_t>(Modifiers::Ctrl);
        auto alt   = static_cast<uint32_t>(Modifiers::Alt);
        auto meta  = static_cast<uint32_t>(Modifiers::Meta);
        auto caps  = static_cast<uint32_t>(Modifiers::CapsLock);
        auto num   = static_cast<uint32_t>(Modifiers::NumLock);
        // All single-bit values
        CHECK((shift & (shift - 1)) == 0);
        CHECK((ctrl  & (ctrl  - 1)) == 0);
        CHECK((alt   & (alt   - 1)) == 0);
        CHECK((meta  & (meta  - 1)) == 0);
        CHECK((caps  & (caps  - 1)) == 0);
        CHECK((num   & (num   - 1)) == 0);
        // All distinct
        CHECK(shift != ctrl); CHECK(shift != alt); CHECK(shift != meta);
        CHECK(ctrl  != alt);  CHECK(ctrl  != meta);
        CHECK(alt   != meta);
    }
    SECTION("operator| combines flags") {
        Modifiers both = Modifiers::Shift | Modifiers::Ctrl;
        CHECK(hasModifier(both, Modifiers::Shift));
        CHECK(hasModifier(both, Modifiers::Ctrl));
        CHECK_FALSE(hasModifier(both, Modifiers::Alt));
    }
    SECTION("hasModifier with None") {
        CHECK_FALSE(hasModifier(Modifiers::None, Modifiers::Shift));
        CHECK_FALSE(hasModifier(Modifiers::None, Modifiers::Ctrl));
    }
    SECTION("hasModifier single flag") {
        CHECK(hasModifier(Modifiers::Meta, Modifiers::Meta));
        CHECK_FALSE(hasModifier(Modifiers::Meta, Modifiers::Shift));
    }
    SECTION("All modifiers combined") {
        Modifiers all = Modifiers::Shift | Modifiers::Ctrl | Modifiers::Alt
                      | Modifiers::Meta | Modifiers::CapsLock | Modifiers::NumLock;
        CHECK(hasModifier(all, Modifiers::Shift));
        CHECK(hasModifier(all, Modifiers::Ctrl));
        CHECK(hasModifier(all, Modifiers::Alt));
        CHECK(hasModifier(all, Modifiers::Meta));
        CHECK(hasModifier(all, Modifiers::CapsLock));
        CHECK(hasModifier(all, Modifiers::NumLock));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("KeyCode — ASCII consistency", "[input][keycode]") {
    SECTION("Letters match ASCII upper-case values") {
        CHECK(static_cast<uint32_t>(KeyCode::A) == 65u);
        CHECK(static_cast<uint32_t>(KeyCode::Z) == 90u);
        // Sequential
        CHECK(static_cast<uint32_t>(KeyCode::B) == 66u);
        CHECK(static_cast<uint32_t>(KeyCode::M) == 77u);
    }
    SECTION("Digits match ASCII values") {
        CHECK(static_cast<uint32_t>(KeyCode::Digit0) == 48u);
        CHECK(static_cast<uint32_t>(KeyCode::Digit9) == 57u);
        CHECK(static_cast<uint32_t>(KeyCode::Digit1) == 49u);
    }
    SECTION("Space matches ASCII") {
        CHECK(static_cast<uint32_t>(KeyCode::Space) == 32u);
    }
    SECTION("Unknown is zero") {
        CHECK(static_cast<uint32_t>(KeyCode::Unknown) == 0u);
    }
    SECTION("Extended codes above 255") {
        CHECK(static_cast<uint32_t>(KeyCode::Escape)    >= 256u);
        CHECK(static_cast<uint32_t>(KeyCode::ArrowLeft) >= 256u);
        const auto f1 = static_cast<uint32_t>(KeyCode::F1);
        CHECK((f1 >= 256u || f1 == 112u));  // F1 is either extended (>255) or ASCII 112
    }
    SECTION("Function keys start at 112") {
        CHECK(static_cast<uint32_t>(KeyCode::F1)  == 112u);
        CHECK(static_cast<uint32_t>(KeyCode::F12) == 123u);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("MouseButton enum values", "[input][mouse]") {
    CHECK(static_cast<uint8_t>(MouseButton::Left)   == 0u);
    CHECK(static_cast<uint8_t>(MouseButton::Right)  == 1u);
    CHECK(static_cast<uint8_t>(MouseButton::Middle) == 2u);
    CHECK(static_cast<uint8_t>(MouseButton::X1)     == 3u);
    CHECK(static_cast<uint8_t>(MouseButton::X2)     == 4u);
}

// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TouchEvent capacity", "[input][touch]") {
    CHECK(TouchEvent::MAX_TOUCHES == 10u);
    CHECK(sizeof(TouchEvent::points) == sizeof(TouchPoint) * 10u);
}

// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("InputEvent — variant holds all types", "[input][variant]") {
    // Verify each event type can be placed in the variant and held correctly
    InputEvent ev;

    ev = MouseMoveEvent{1.0f, 2.0f, 0.1f, 0.2f, Modifiers::None};
    CHECK(std::holds_alternative<MouseMoveEvent>(ev));
    CHECK(std::get<MouseMoveEvent>(ev).x == 1.0f);
    CHECK(std::get<MouseMoveEvent>(ev).y == 2.0f);

    ev = MouseButtonEvent{10.0f, 20.0f, MouseButton::Right, true, 1, Modifiers::Shift};
    CHECK(std::holds_alternative<MouseButtonEvent>(ev));
    CHECK(std::get<MouseButtonEvent>(ev).button == MouseButton::Right);
    CHECK(std::get<MouseButtonEvent>(ev).pressed);

    ev = ScrollEvent{5.0f, 6.0f, -1.0f, 3.0f, true, Modifiers::None};
    CHECK(std::holds_alternative<ScrollEvent>(ev));
    CHECK(std::get<ScrollEvent>(ev).isTrackpad);

    ev = KeyEvent{KeyCode::A, 0u, true, false, Modifiers::Ctrl};
    CHECK(std::holds_alternative<KeyEvent>(ev));
    CHECK(std::get<KeyEvent>(ev).key == KeyCode::A);
    CHECK(hasModifier(std::get<KeyEvent>(ev).modifiers, Modifiers::Ctrl));

    ev = TextInputEvent{U'’'};  // RIGHT SINGLE QUOTATION MARK
    CHECK(std::holds_alternative<TextInputEvent>(ev));
    CHECK(std::get<TextInputEvent>(ev).codepoint == U'’');

    ev = StylusEvent{50.0f, 60.0f, 0.75f, 10.0f, -5.0f, 45.0f, false, true};
    CHECK(std::holds_alternative<StylusEvent>(ev));
    CHECK(std::get<StylusEvent>(ev).pressure == 0.75f);
    CHECK(std::get<StylusEvent>(ev).inProximity);

    ev = WindowResizeEvent{1920u, 1080u, 2.0f};
    CHECK(std::holds_alternative<WindowResizeEvent>(ev));
    CHECK(std::get<WindowResizeEvent>(ev).devicePixelRatio == 2.0f);

    ev = WindowFocusEvent{true};
    CHECK(std::holds_alternative<WindowFocusEvent>(ev));
    CHECK(std::get<WindowFocusEvent>(ev).gained);
}

// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("InputEvent — std::visit dispatch", "[input][variant]") {
    InputEvent ev = KeyEvent{KeyCode::Z, 44u, true, true, Modifiers::Meta};

    bool dispatched = false;
    std::visit([&dispatched](const auto& e) {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, KeyEvent>) {
            dispatched = true;
            CHECK(e.key == KeyCode::Z);
            CHECK(e.repeat);
            CHECK(hasModifier(e.modifiers, Modifiers::Meta));
        }
    }, ev);

    CHECK(dispatched);
}
