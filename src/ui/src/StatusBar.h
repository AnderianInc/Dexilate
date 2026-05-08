// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include <QStatusBar>

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace dexilate::ui {

// Status bar: cursor canvas coordinates, zoom level, document dimensions.
class DexStatusBar : public QStatusBar {
    Q_OBJECT
public:
    explicit DexStatusBar(QWidget* parent = nullptr);

public slots:
    void setCursorPosition(float x, float y);
    void setZoom(float zoom);
    void setDocumentSize(uint32_t w, uint32_t h);
    void setModified(bool modified);

private:
    QLabel* _posLabel;
    QLabel* _zoomLabel;
    QLabel* _sizeLabel;
    QLabel* _modLabel;
};

} // namespace dexilate::ui
