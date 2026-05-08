// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/app/main.cpp
//
//  Phase 1 — Qt6 application entry point.
//
//  Starts QApplication, creates the DexilateApp lifecycle object (which owns
//  the MainWindow and initial document), then enters the Qt event loop.
// ─────────────────────────────────────────────────────────────────────────────

#include "DexilateApp.h"

#include <QApplication>
#include <QSurfaceFormat>
#include <cstdlib>

int main(int argc, char* argv[]) {
    // High-DPI scaling is enabled by default in Qt6; no explicit attribute needed.
    QApplication app(argc, argv);
    app.setApplicationName("Dexilate");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Dexilate Software");

    dexilate::DexilateApp dexApp;
    dexApp.show();

    return app.exec();
}
