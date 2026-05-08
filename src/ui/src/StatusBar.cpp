// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "StatusBar.h"

#include <QLabel>

namespace dexilate::ui {

DexStatusBar::DexStatusBar(QWidget* parent) : QStatusBar(parent) {
    _posLabel  = new QLabel("X: 0  Y: 0", this);
    _zoomLabel = new QLabel("100%", this);
    _sizeLabel = new QLabel("", this);
    _modLabel  = new QLabel("", this);

    _posLabel->setMinimumWidth(120);
    _zoomLabel->setMinimumWidth(60);

    addPermanentWidget(_modLabel);
    addPermanentWidget(_sizeLabel);
    addPermanentWidget(_zoomLabel);
    addPermanentWidget(_posLabel);
}

void DexStatusBar::setCursorPosition(float x, float y) {
    _posLabel->setText(QString("X: %1  Y: %2")
        .arg(static_cast<int>(x))
        .arg(static_cast<int>(y)));
}

void DexStatusBar::setZoom(float zoom) {
    _zoomLabel->setText(QString("%1%").arg(static_cast<int>(zoom * 100)));
}

void DexStatusBar::setDocumentSize(uint32_t w, uint32_t h) {
    _sizeLabel->setText(QString("%1 × %2 px").arg(w).arg(h));
}

void DexStatusBar::setModified(bool modified) {
    _modLabel->setText(modified ? "●" : "");
}

} // namespace dexilate::ui
