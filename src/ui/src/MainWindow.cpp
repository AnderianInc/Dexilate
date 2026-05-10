// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "MainWindow.h"
#include "CanvasWidget.h"
#include "ToolOptionsBar.h"
#include "StatusBar.h"
#include "LayerPanel.h"
#include "HistoryPanel.h"

#include "dexilate/engine/document/Document.h"
#include "dexilate/core/codecs/PngCodec.h"

#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>

namespace dexilate::ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Dexilate");
    resize(1280, 800);

    // ── Canvas ────────────────────────────────────────────────────────────────
    _canvas = new CanvasWidget(this);
    setCentralWidget(_canvas);
    connect(_canvas, &CanvasWidget::documentModified, this, &MainWindow::onDocumentModified);
    connect(_canvas, &CanvasWidget::cursorMoved,      this, &MainWindow::onCursorMoved);

    // ── Tool options toolbar ──────────────────────────────────────────────────
    _toolBar = new ToolOptionsBar(this);
    addToolBar(Qt::TopToolBarArea, _toolBar);
    _toolBar->bindSettings(&_canvas->brushSettings());

    // ── Status bar ────────────────────────────────────────────────────────────
    _statusBar = new DexStatusBar(this);
    setStatusBar(_statusBar);

    // ── Dock panels ──────────────────────────────────────────────────────────
    _layerPanel = new LayerPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, _layerPanel);
    connect(_layerPanel, &LayerPanel::layerSelected, this, [this](int) {
        _canvas->update();
    });
    connect(_layerPanel, &LayerPanel::layerAdded, this, [this]() {
        _canvas->update();
        _doc->markModified();
        updateTitle();
    });
    connect(_layerPanel, &LayerPanel::layerRemoved, this, [this](int) {
        _canvas->update();
        _doc->markModified();
        updateTitle();
    });

    _historyPanel = new HistoryPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, _historyPanel);
    _historyPanel->setHistoryManager(&_history);

    // ── Menus ─────────────────────────────────────────────────────────────────
    QMenu* fileMenu = menuBar()->addMenu("&File");
    auto* actNew    = fileMenu->addAction("&New",        QKeySequence::New,       this, &MainWindow::onNew);
    auto* actOpen   = fileMenu->addAction("&Open...",    QKeySequence::Open,      this, &MainWindow::onOpen);
    fileMenu->addSeparator();
    auto* actSave   = fileMenu->addAction("&Save",       QKeySequence::Save,      this, &MainWindow::onSave);
    auto* actSaveAs = fileMenu->addAction("Save &As...", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S),
                                          this, &MainWindow::onSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction("&Quit", QKeySequence::Quit, this, &QWidget::close);
    (void)actNew; (void)actOpen; (void)actSave; (void)actSaveAs;

    QMenu* editMenu = menuBar()->addMenu("&Edit");
    _actUndo = editMenu->addAction("&Undo", QKeySequence::Undo, this, &MainWindow::onUndo);
    _actRedo = editMenu->addAction("&Redo", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z),
                                   this, &MainWindow::onRedo);
    editMenu->addSeparator();
    editMenu->addAction("Cu&t",   QKeySequence::Cut,   this, &MainWindow::onCut);
    editMenu->addAction("&Copy",  QKeySequence::Copy,  this, &MainWindow::onCopy);
    editMenu->addAction("&Paste", QKeySequence::Paste, this, &MainWindow::onPaste);

    _actUndo->setEnabled(false);
    _actRedo->setEnabled(false);

    QMenu* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("Zoom &In",       QKeySequence(Qt::CTRL | Qt::Key_Equal), this, &MainWindow::onZoomIn);
    viewMenu->addAction("Zoom &Out",      QKeySequence(Qt::CTRL | Qt::Key_Minus), this, &MainWindow::onZoomOut);
    viewMenu->addAction("&Fit to Window", QKeySequence(Qt::CTRL | Qt::Key_0),     this, &MainWindow::onFitWindow);
}

void MainWindow::setDocument(std::unique_ptr<engine::Document> doc) {
    _doc = std::move(doc);
    applyDocument();
}

void MainWindow::applyDocument() {
    _canvas->setDocument(_doc.get());
    _layerPanel->setDocument(_doc.get());
    if (_doc) {
        _statusBar->setDocumentSize(_doc->width(), _doc->height());
        _statusBar->setZoom(_canvas->viewTransform().zoom());
        _statusBar->setModified(_doc->isModified());
    }
    updateTitle();
}

void MainWindow::updateTitle() {
    if (!_doc) { setWindowTitle("Dexilate"); return; }
    QString mod  = _doc->isModified() ? "● " : "";
    QString name = QString::fromStdString(_doc->title());
    setWindowTitle(mod + name + " — Dexilate");
}

// ── File actions ──────────────────────────────────────────────────────────────
void MainWindow::onNew() {
    if (!maybeSave()) return;
    _doc = std::make_unique<engine::Document>(1920u, 1080u, "Untitled");
    _history.clear();
    onHistoryChanged();
    applyDocument();
}

void MainWindow::onOpen() {
    if (!maybeSave()) return;
    QString path = QFileDialog::getOpenFileName(
        this, "Open Image", {},
        "PNG Images (*.png);;All Files (*)");
    if (path.isEmpty()) return;

    try {
        core::PngCodec codec;
        auto img = codec.decode(std::filesystem::path(path.toStdString()));
        _doc = std::make_unique<engine::Document>(
            engine::Document::fromImage(img, std::filesystem::path(path.toStdString())));
        _history.clear();
        onHistoryChanged();
        applyDocument();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Open Failed", e.what());
    }
}

void MainWindow::onSave() {
    if (!_doc) return;
    if (_doc->filePath().empty()) { onSaveAs(); return; }
    saveToPath(_doc->filePath());
}

void MainWindow::onSaveAs() {
    if (!_doc) return;
    QString path = QFileDialog::getSaveFileName(
        this, "Save As", {}, "PNG Images (*.png)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".png", Qt::CaseInsensitive))
        path += ".png";
    if (saveToPath(std::filesystem::path(path.toStdString())))
        _doc->setFilePath(std::filesystem::path(path.toStdString()));
}

bool MainWindow::saveToPath(const std::filesystem::path& path) {
    if (!_doc) return false;
    auto* layer = _doc->activeLayer();
    if (!layer) return false;
    try {
        std::vector<uint8_t> buf(static_cast<size_t>(_doc->width()) * _doc->height() * 4);
        layer->composite(buf.data(), _doc->width(), _doc->height());
        core::PngCodec codec;
        codec.encode(path, _doc->width(), _doc->height(), buf.data());
        _doc->clearModified();
        updateTitle();
        _statusBar->setModified(false);
        return true;
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save Failed", e.what());
        return false;
    }
}

bool MainWindow::maybeSave() {
    if (!_doc || !_doc->isModified()) return true;
    auto btn = QMessageBox::question(
        this, "Unsaved Changes",
        "Save changes to \"" + QString::fromStdString(_doc->title()) + "\"?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (btn == QMessageBox::Cancel) return false;
    if (btn == QMessageBox::Save)   onSave();
    return true;
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (maybeSave()) e->accept();
    else             e->ignore();
}

// ── View actions ──────────────────────────────────────────────────────────────
void MainWindow::onZoomIn()    { _canvas->zoomIn();      _statusBar->setZoom(_canvas->viewTransform().zoom()); }
void MainWindow::onZoomOut()   { _canvas->zoomOut();     _statusBar->setZoom(_canvas->viewTransform().zoom()); }
void MainWindow::onFitWindow() { _canvas->fitToWindow(); _statusBar->setZoom(_canvas->viewTransform().zoom()); }

void MainWindow::onDocumentModified() {
    _statusBar->setModified(true);
    updateTitle();
}

void MainWindow::onCursorMoved(float x, float y) {
    _statusBar->setCursorPosition(x, y);
}

// ── Edit actions ──────────────────────────────────────────────────────────────
void MainWindow::onUndo() {
    if (!_history.canUndo()) return;
    _history.undo();
    onHistoryChanged();
    _canvas->update();
}

void MainWindow::onRedo() {
    if (!_history.canRedo()) return;
    _history.redo();
    onHistoryChanged();
    _canvas->update();
}

void MainWindow::onCut() {
    _statusBar->showMessage("Cut: not yet implemented", 2000);
}

void MainWindow::onCopy() {
    _statusBar->showMessage("Copy: not yet implemented", 2000);
}

void MainWindow::onPaste() {
    _statusBar->showMessage("Paste: not yet implemented", 2000);
}

void MainWindow::onHistoryChanged() {
    _actUndo->setEnabled(_history.canUndo());
    _actRedo->setEnabled(_history.canRedo());
    _historyPanel->refresh();
}

} // namespace dexilate::ui
