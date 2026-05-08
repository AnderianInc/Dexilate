// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "CanvasWidget.h"
#include "dexilate/engine/document/Document.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTabletEvent>
#include <QResizeEvent>

namespace dexilate::ui {

CanvasWidget::CanvasWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_TabletTracking);
    setFocusPolicy(Qt::StrongFocus);
    // Neutral canvas background
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0xA0, 0xA0, 0xA0));
    setPalette(pal);
    setAutoFillBackground(true);
}

void CanvasWidget::setDocument(engine::Document* doc) {
    _doc = doc;
    if (doc && width() > 0 && height() > 0)
        _view.fitToViewport(static_cast<float>(doc->width()),
                            static_cast<float>(doc->height()),
                            static_cast<float>(width()),
                            static_cast<float>(height()));
    update();
}

// ── Zoom helpers ──────────────────────────────────────────────────────────────
void CanvasWidget::zoomIn() {
    glm::vec2 centre{width() * 0.5f, height() * 0.5f};
    _view.zoomAround(centre, 1.25f);
    update();
}
void CanvasWidget::zoomOut() {
    glm::vec2 centre{width() * 0.5f, height() * 0.5f};
    _view.zoomAround(centre, 0.8f);
    update();
}
void CanvasWidget::fitToWindow() {
    if (!_doc) return;
    _view.fitToViewport(static_cast<float>(_doc->width()),
                        static_cast<float>(_doc->height()),
                        static_cast<float>(width()),
                        static_cast<float>(height()));
    update();
}

// ── Paint event ───────────────────────────────────────────────────────────────
void CanvasWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (!_doc) return;

    auto* layer = _doc->activeLayer();
    if (!layer) return;

    // Composite the active layer into a QImage (RGBA8)
    QImage img(static_cast<int>(layer->width()),
               static_cast<int>(layer->height()),
               QImage::Format_RGBA8888);
    layer->composite(img.bits(), layer->width(), layer->height());

    // Apply view transform: translate then scale
    auto pan  = _view.panOffset();
    float z   = _view.zoom();
    painter.translate(static_cast<double>(pan.x), static_cast<double>(pan.y));
    painter.scale(static_cast<double>(z), static_cast<double>(z));
    painter.drawImage(0, 0, img);
}

void CanvasWidget::resizeEvent(QResizeEvent*) {
    if (_doc)
        _view.fitToViewport(static_cast<float>(_doc->width()),
                            static_cast<float>(_doc->height()),
                            static_cast<float>(width()),
                            static_cast<float>(height()));
}

// ── Mouse events ──────────────────────────────────────────────────────────────
void CanvasWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        auto p = e->position();
        beginPaint(static_cast<float>(p.x()), static_cast<float>(p.y()), 1.0f);
    }
}
void CanvasWidget::mouseMoveEvent(QMouseEvent* e) {
    auto p = e->position();
    float x = static_cast<float>(p.x());
    float y = static_cast<float>(p.y());
    auto cp = _view.screenToCanvas({x, y});
    emit cursorMoved(cp.x, cp.y);
    if (_painting)
        continuePaint(x, y, 1.0f);
}
void CanvasWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton)
        endPaint();
}

void CanvasWidget::wheelEvent(QWheelEvent* e) {
    float delta = static_cast<float>(e->angleDelta().y()) / 120.0f;
    float factor = (delta > 0) ? 1.15f : (1.0f / 1.15f);
    auto pos = e->position();
    _view.zoomAround({static_cast<float>(pos.x()), static_cast<float>(pos.y())}, factor);
    update();
}

void CanvasWidget::tabletEvent(QTabletEvent* e) {
    float x = static_cast<float>(e->position().x());
    float y = static_cast<float>(e->position().y());
    float p = static_cast<float>(e->pressure());

    switch (e->type()) {
    case QEvent::TabletPress:   beginPaint(x, y, p);    break;
    case QEvent::TabletMove:    continuePaint(x, y, p); break;
    case QEvent::TabletRelease: endPaint();              break;
    default: break;
    }
    e->accept();
}

// ── Brush dispatch ────────────────────────────────────────────────────────────
void CanvasWidget::beginPaint(float sx, float sy, float pressure) {
    if (!_doc) return;
    auto* layer = _doc->activeLayer();
    if (!layer) return;

    _painting = true;
    _residual = 0.0f;
    auto cp = _view.screenToCanvas({sx, sy});
    _lastX = cp.x; _lastY = cp.y; _lastP = pressure;

    engine::BrushDab dab{cp.x, cp.y, pressure};
    _brushEngine.paintDab(*layer, dab, _brushSettings);
    _doc->markModified();
    update();
}

void CanvasWidget::continuePaint(float sx, float sy, float pressure) {
    if (!_painting || !_doc) return;
    auto* layer = _doc->activeLayer();
    if (!layer) return;

    auto cp = _view.screenToCanvas({sx, sy});
    engine::BrushDab prev{_lastX, _lastY, _lastP};
    engine::BrushDab curr{cp.x,   cp.y,   pressure};
    _residual = _brushEngine.paintSegment(*layer, prev, curr, _brushSettings, _residual);

    _lastX = cp.x; _lastY = cp.y; _lastP = pressure;
    emit documentModified();
    update();
}

void CanvasWidget::endPaint() {
    _painting = false;
    _residual = 0.0f;
}

} // namespace dexilate::ui
