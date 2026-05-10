// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include <QDockWidget>
#include <QListWidget>
#include <QPushButton>

namespace dexilate::engine { class Document; }

namespace dexilate::ui {

class LayerPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit LayerPanel(QWidget* parent = nullptr);

    void setDocument(engine::Document* doc);

signals:
    void layerSelected(int index);
    void layerAdded();
    void layerRemoved(int index);

private slots:
    void onItemSelectionChanged();
    void onAddLayer();
    void onDeleteLayer();
    void onDuplicateLayer();
    void onMoveUp();
    void onMoveDown();

private:
    void rebuildList();

    engine::Document* _doc = nullptr;

    QListWidget* _list      = nullptr;
    QPushButton* _btnAdd    = nullptr;
    QPushButton* _btnDelete = nullptr;
    QPushButton* _btnDup    = nullptr;
    QPushButton* _btnUp     = nullptr;
    QPushButton* _btnDown   = nullptr;
};

} // namespace dexilate::ui
