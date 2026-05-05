// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  dexilate/platform/Platform.h
//
//  Top-level Platform Abstraction Layer lifecycle.
//
//  Call Platform::init() once at application startup (before creating any
//  IWindow, IFileSystem, or IClipboard) and Platform::shutdown() before exit.
//
//  When Qt6 is active (Phase 1+), Qt initialises its own platform backend
//  before this code runs. Platform::init() detects that and skips redundant
//  initialisation.
// ─────────────────────────────────────────────────────────────────────────────

namespace dexilate::platform {

class Platform {
public:
    Platform()  = delete;

    /// Initialise the platform subsystem. Must be called from the main thread
    /// before any PAL object is created.
    static void init();

    /// Release platform resources. Must be called from the main thread after
    /// all PAL objects have been destroyed.
    static void shutdown();

    /// True after init() has been called and before shutdown() returns.
    [[nodiscard]] static bool isInitialised();
};

} // namespace dexilate::platform
