// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "dexilate/engine/raster/BrushEngine.h"
#include "dexilate/engine/raster/Eraser.h"
#include "dexilate/engine/viewport/ViewTransform.h"

#include <QWidget>
#include <QImage>

namespace dexilate::engine { class Document; }

namespace dexilate::ui {

// Central canvas widget. Renders the active document layer via QPainter (Phase 1
// software path). Handles mouse and tablet events, dispatching dabs to the
// BrushEngine and requesting repaints.
class CanvasWidget : public QWidget {
    Q_OBJECT
public:
    explicit CanvasWidget(QWidget* parent = nullptr);

    void setDocument(engine::Document* doc);
    void setBrushSettings(const engine::BrushSettings& s) { _brushSettings = s; }
    engine::BrushSettings& brushSettings() noexcept { return _brushSettings; }

    engine::ViewTransform& viewTransform() noexcept { return _view; }

    // Zoom around the canvas centre
    void zoomIn();
    void zoomOut();
    void fitToWindow();

signals:
    void cursorMoved(float canvasX, float canvasY);
    void documentModified();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void tabletEvent(QTabletEvent* event) override;

private:
    void beginPaint(float x, float y, float pressure);
    void continuePaint(float x, float y, float pressure);
    void endPaint();

    engine::Document*     _doc           = nullptr;
    engine::BrushEngine   _brushEngine;
    engine::Eraser        _eraser;
    engine::BrushSettings _brushSettings;
    engine::ViewTransform _view;

    bool  _painting    = false;
    float _lastX       = 0.0f;
    float _lastY       = 0.0f;
    float _lastP       = 1.0f;
    float _residual    = 0.0f;
};

} // namespace dexilate::ui
