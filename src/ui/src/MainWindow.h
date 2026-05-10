// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "dexilate/engine/document/Document.h"
#include "dexilate/engine/document/HistoryManager.h"

#include <QMainWindow>
#include <QAction>
#include <memory>
#include <filesystem>

namespace dexilate::ui {

class CanvasWidget;
class ToolOptionsBar;
class DexStatusBar;
class LayerPanel;
class HistoryPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Transfer ownership of the initial document.
    void setDocument(std::unique_ptr<engine::Document> doc);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onZoomIn();
    void onZoomOut();
    void onFitWindow();
    void onDocumentModified();
    void onCursorMoved(float x, float y);
    void onUndo();
    void onRedo();
    void onCut();
    void onCopy();
    void onPaste();
    void onHistoryChanged();

private:
    bool maybeSave();
    bool saveToPath(const std::filesystem::path& path);
    void updateTitle();
    void applyDocument();

    std::unique_ptr<engine::Document> _doc;
    engine::HistoryManager            _history;

    CanvasWidget*   _canvas      = nullptr;
    ToolOptionsBar* _toolBar     = nullptr;
    DexStatusBar*   _statusBar   = nullptr;
    LayerPanel*     _layerPanel  = nullptr;
    HistoryPanel*   _historyPanel = nullptr;

    QAction* _actUndo = nullptr;
    QAction* _actRedo = nullptr;
};

} // namespace dexilate::ui
