// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/platform/src/Platform.cpp
//
//  Cross-platform PAL lifecycle. Compiled on all platforms.
//  OS-specific init (NSApp, CoInitialize, X11 Display) lives in the
//  respective IWindow::create() factory, not here, so each platform file
//  is self-contained and this file remains dependency-free.
// ─────────────────────────────────────────────────────────────────────────────

#include "dexilate/platform/Platform.h"
#include <atomic>

namespace dexilate::platform {

namespace {
    std::atomic<bool> g_initialised{false};
}

void Platform::init() {
    g_initialised.store(true, std::memory_order_release);
}

void Platform::shutdown() {
    g_initialised.store(false, std::memory_order_release);
}

bool Platform::isInitialised() {
    return g_initialised.load(std::memory_order_acquire);
}

} // namespace dexilate::platform
