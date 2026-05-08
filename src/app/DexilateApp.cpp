// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "DexilateApp.h"
#include "dexilate/engine/document/Document.h"
#include "MainWindow.h"

namespace dexilate {

DexilateApp::DexilateApp() {
    _mainWindow = std::make_unique<ui::MainWindow>();

    // Start with a default blank canvas (1920×1080)
    auto doc = std::make_unique<engine::Document>(1920u, 1080u, "Untitled");
    _mainWindow->setDocument(std::move(doc));
}

DexilateApp::~DexilateApp() = default;

void DexilateApp::show() {
    _mainWindow->show();
}

} // namespace dexilate
