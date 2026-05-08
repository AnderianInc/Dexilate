// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "ToolOptionsBar.h"

#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QWidget>

namespace dexilate::ui {

ToolOptionsBar::ToolOptionsBar(QWidget* parent)
    : QToolBar("Tool Options", parent)
{
    setMovable(false);

    // ── Brush size ────────────────────────────────────────────────────────────
    addWidget(new QLabel("Size:", this));
    _sizeSlider = new QSlider(Qt::Horizontal, this);
    _sizeSlider->setRange(1, 500);
    _sizeSlider->setValue(20);
    _sizeSlider->setFixedWidth(100);
    _sizeLabel = new QLabel("20px", this);
    _sizeLabel->setFixedWidth(40);
    addWidget(_sizeSlider);
    addWidget(_sizeLabel);
    addSeparator();

    // ── Opacity ───────────────────────────────────────────────────────────────
    addWidget(new QLabel("Opacity:", this));
    _opacitySlider = new QSlider(Qt::Horizontal, this);
    _opacitySlider->setRange(1, 100);
    _opacitySlider->setValue(100);
    _opacitySlider->setFixedWidth(100);
    _opacityLabel = new QLabel("100%", this);
    _opacityLabel->setFixedWidth(40);
    addWidget(_opacitySlider);
    addWidget(_opacityLabel);
    addSeparator();

    // ── Colour swatch ─────────────────────────────────────────────────────────
    addWidget(new QLabel("Color:", this));
    _colorButton = new QPushButton(this);
    _colorButton->setFixedSize(28, 24);
    _colorButton->setStyleSheet("background-color: black; border: 1px solid #888;");
    addWidget(_colorButton);

    connect(_sizeSlider,    &QSlider::valueChanged, this, &ToolOptionsBar::onSizeChanged);
    connect(_opacitySlider, &QSlider::valueChanged, this, &ToolOptionsBar::onOpacityChanged);
    connect(_colorButton,   &QPushButton::clicked,  this, &ToolOptionsBar::onColorPick);
}

void ToolOptionsBar::bindSettings(engine::BrushSettings* settings) {
    _settings = settings;
}

void ToolOptionsBar::onSizeChanged(int value) {
    _sizeLabel->setText(QString::number(value) + "px");
    if (_settings) {
        _settings->baseSize = static_cast<float>(value);
        emit settingsChanged();
    }
}

void ToolOptionsBar::onOpacityChanged(int value) {
    _opacityLabel->setText(QString::number(value) + "%");
    if (_settings) {
        _settings->baseOpacity = value / 100.0f;
        emit settingsChanged();
    }
}

void ToolOptionsBar::onColorPick() {
    QColor c = QColorDialog::getColor(_currentColor, this, "Brush Color");
    if (!c.isValid()) return;
    _currentColor = c;
    _colorButton->setStyleSheet(
        QString("background-color: %1; border: 1px solid #888;").arg(c.name()));
    if (_settings) {
        _settings->color = {c.redF(), c.greenF(), c.blueF(), c.alphaF()};
        emit settingsChanged();
    }
}

} // namespace dexilate::ui
