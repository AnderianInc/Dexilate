// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "dexilate/engine/raster/BrushEngine.h"

#include <QToolBar>

QT_BEGIN_NAMESPACE
class QSlider;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace dexilate::ui {

// Toolbar that exposes brush size, opacity, hardness, and colour.
// Changes are applied immediately to the BrushSettings pointer.
class ToolOptionsBar : public QToolBar {
    Q_OBJECT
public:
    explicit ToolOptionsBar(QWidget* parent = nullptr);

    // Attach to the brush settings owned by CanvasWidget.
    void bindSettings(engine::BrushSettings* settings);

signals:
    void settingsChanged();

private slots:
    void onSizeChanged(int value);
    void onOpacityChanged(int value);
    void onColorPick();

private:
    engine::BrushSettings* _settings = nullptr;

    QSlider*       _sizeSlider    = nullptr;
    QLabel*        _sizeLabel     = nullptr;
    QSlider*       _opacitySlider = nullptr;
    QLabel*        _opacityLabel  = nullptr;
    QPushButton*   _colorButton   = nullptr;
    QColor         _currentColor  = Qt::black;
};

} // namespace dexilate::ui
