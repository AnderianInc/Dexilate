// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include <memory>

namespace dexilate::ui { class MainWindow; }

namespace dexilate {

// Application lifecycle object. Created in main(); owns the MainWindow.
// Phase 1: creates a blank document and shows the main window.
// Phase 2+: session restore, recent-files startup, plugin loading.
class DexilateApp {
public:
    DexilateApp();
    ~DexilateApp();

    void show();

private:
    std::unique_ptr<ui::MainWindow> _mainWindow;
};

} // namespace dexilate
