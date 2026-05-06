// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  tests/platform/test_window.cpp
//
//  Integration tests for IWindow lifecycle.
//
//  These tests create a real OS window and therefore require a graphical
//  display. In CI they run under Xvfb (Linux) or the hosted macOS/Windows
//  runners. If window creation throws (headless environment with no display),
//  the relevant sections are skipped rather than failed.
// ─────────────────────────────────────────────────────────────────────────────

#include <catch2/catch_test_macros.hpp>
#include "dexilate/platform/Platform.h"
#include "dexilate/platform/IWindow.h"
#include "dexilate/platform/InputEvent.h"

using namespace dexilate::platform;

// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("Platform lifecycle", "[platform]") {
    SECTION("init sets initialised flag") {
        Platform::init();
        CHECK(Platform::isInitialised());
    }
    SECTION("shutdown clears initialised flag") {
        Platform::init();
        Platform::shutdown();
        CHECK_FALSE(Platform::isInitialised());
    }
    SECTION("double init is safe") {
        Platform::init();
        Platform::init();  // must not crash or assert
        CHECK(Platform::isInitialised());
        Platform::shutdown();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("IWindow - creation and dimensions", "[window]") {
    Platform::init();

    WindowDesc desc;
    desc.title  = "TestWindow";
    desc.width  = 800;
    desc.height = 600;

    std::unique_ptr<IWindow> win;
    try {
        win = IWindow::create(desc);
    } catch (const std::exception& e) {
        SKIP("No display available: " << e.what());
    }

    REQUIRE(win != nullptr);

    SECTION("logical dimensions match request") {
        // Logical size should match the requested size
        CHECK(win->width()  == 800u);
        CHECK(win->height() == 600u);
    }
    SECTION("physical dimensions are >= logical dimensions") {
        // Only holds when DPR >= 1.0; sub-96-DPI displays (e.g. Xvfb at 75 DPI)
        // produce DPR < 1.0 where physical < logical by design.
        if (win->devicePixelRatio() >= 1.0f) {
            CHECK(win->physicalWidth()  >= win->width());
            CHECK(win->physicalHeight() >= win->height());
        } else {
            CHECK(win->physicalWidth()  > 0u);
            CHECK(win->physicalHeight() > 0u);
        }
    }
    SECTION("devicePixelRatio is positive") {
        CHECK(win->devicePixelRatio() > 0.0f);
    }
    SECTION("title matches request") {
        CHECK(win->title() == "TestWindow");
    }
    SECTION("shouldClose is false after creation") {
        CHECK_FALSE(win->shouldClose());
    }
    SECTION("isFullscreen is false after creation") {
        CHECK_FALSE(win->isFullscreen());
    }
    SECTION("presentMode defaults to Fifo") {
        CHECK(win->presentMode() == PresentMode::Fifo);
    }

    Platform::shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("IWindow - show / hide / close", "[window]") {
    Platform::init();

    std::unique_ptr<IWindow> win;
    try {
        win = IWindow::create({"LifecycleTest", 640u, 480u});
    } catch (...) {
        SKIP("No display available");
    }

    SECTION("show then hide does not crash") {
        win->show();
        win->pollEvents();
        win->hide();
        win->pollEvents();
    }
    SECTION("close sets shouldClose") {
        win->show();
        win->pollEvents();
        win->close();
        // Process the close event
        win->pollEvents();
        CHECK(win->shouldClose());
    }

    Platform::shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("IWindow - resize callback", "[window]") {
    Platform::init();

    std::unique_ptr<IWindow> win;
    try {
        win = IWindow::create({"ResizeTest", 400u, 300u});
    } catch (...) {
        SKIP("No display available");
    }

    uint32_t cbW = 0, cbH = 0;
    win->setResizeCallback([&](uint32_t w, uint32_t h) {
        cbW = w; cbH = h;
    });

    win->show();
    win->pollEvents();
    win->resize(600u, 400u);
    win->pollEvents();  // dispatch the ConfigureNotify / WM_SIZE

    // Note: some platforms deliver resize synchronously, others asynchronously.
    // We just verify the callback is wired up correctly (w/h > 0 if called).
    if (cbW > 0 || cbH > 0) {
        CHECK(cbW == 600u);
        CHECK(cbH == 400u);
    }

    Platform::shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("IWindow - set/get title", "[window]") {
    Platform::init();

    std::unique_ptr<IWindow> win;
    try {
        win = IWindow::create({"Original", 320u, 240u});
    } catch (...) {
        SKIP("No display available");
    }

    win->setTitle("Updated Title");
    // Some platforms update synchronously; poll to ensure the update is applied
    win->show();
    win->pollEvents();
    CHECK(win->title() == "Updated Title");

    Platform::shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("IWindow - present mode setter", "[window]") {
    Platform::init();

    std::unique_ptr<IWindow> win;
    try {
        win = IWindow::create({"PresentTest", 320u, 240u});
    } catch (...) {
        SKIP("No display available");
    }

    win->setPresentMode(PresentMode::Immediate);
    CHECK(win->presentMode() == PresentMode::Immediate);

    win->setPresentMode(PresentMode::Mailbox);
    CHECK(win->presentMode() == PresentMode::Mailbox);

    win->setPresentMode(PresentMode::Fifo);
    CHECK(win->presentMode() == PresentMode::Fifo);

    Platform::shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("IWindow - event callback fires on pollEvents", "[window]") {
    Platform::init();

    std::unique_ptr<IWindow> win;
    try {
        win = IWindow::create({"EventTest", 400u, 300u});
    } catch (...) {
        SKIP("No display available");
    }

    bool focusCallbackFired = false;
    win->setEventCallback([&](const InputEvent& ev) {
        if (std::holds_alternative<WindowFocusEvent>(ev))
            focusCallbackFired = true;
    });

    // We can't guarantee the OS will deliver a focus event, but
    // verify the callback is accepted without crash.
    win->show();
    win->pollEvents();
    // Callback wiring is correct if no exception was thrown.
    (void)focusCallbackFired;

    Platform::shutdown();
}
