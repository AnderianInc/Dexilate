// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "LayerPanel.h"
#include "dexilate/engine/document/Document.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>

namespace dexilate::ui {

LayerPanel::LayerPanel(QWidget* parent)
    : QDockWidget("Layers", parent)
{
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QWidget* container = new QWidget(this);
    QVBoxLayout* vlay  = new QVBoxLayout(container);
    vlay->setContentsMargins(4, 4, 4, 4);
    vlay->setSpacing(4);

    _list = new QListWidget(container);
    vlay->addWidget(_list);

    QHBoxLayout* btnRow = new QHBoxLayout;
    btnRow->setSpacing(2);
    _btnAdd    = new QPushButton("+",   container);
    _btnDelete = new QPushButton("−",   container);
    _btnDup    = new QPushButton("⧉",  container);
    _btnUp     = new QPushButton("↑",   container);
    _btnDown   = new QPushButton("↓",   container);
    btnRow->addWidget(_btnAdd);
    btnRow->addWidget(_btnDelete);
    btnRow->addWidget(_btnDup);
    btnRow->addStretch();
    btnRow->addWidget(_btnUp);
    btnRow->addWidget(_btnDown);
    vlay->addLayout(btnRow);

    setWidget(container);

    connect(_list,      &QListWidget::itemSelectionChanged, this, &LayerPanel::onItemSelectionChanged);
    connect(_btnAdd,    &QPushButton::clicked,              this, &LayerPanel::onAddLayer);
    connect(_btnDelete, &QPushButton::clicked,              this, &LayerPanel::onDeleteLayer);
    connect(_btnDup,    &QPushButton::clicked,              this, &LayerPanel::onDuplicateLayer);
    connect(_btnUp,     &QPushButton::clicked,              this, &LayerPanel::onMoveUp);
    connect(_btnDown,   &QPushButton::clicked,              this, &LayerPanel::onMoveDown);
}

void LayerPanel::setDocument(engine::Document* doc) {
    _doc = doc;
    rebuildList();
}

void LayerPanel::rebuildList() {
    QSignalBlocker blocker(_list);
    _list->clear();
    if (!_doc) return;

    // Display layers bottom-to-top (Photoshop convention): top of list = highest index
    int count = _doc->layerCount();
    for (int i = count - 1; i >= 0; --i) {
        auto* layer = _doc->layerAt(i);
        QString name = layer ? QString::fromStdString(layer->name()) : QStringLiteral("Layer %1").arg(i);
        auto* item = new QListWidgetItem(name, _list);
        item->setData(Qt::UserRole, i);
    }

    // Highlight the active layer
    int active = _doc->activeIndex();
    for (int row = 0; row < _list->count(); ++row) {
        if (_list->item(row)->data(Qt::UserRole).toInt() == active) {
            _list->setCurrentRow(row);
            break;
        }
    }
}

void LayerPanel::onItemSelectionChanged() {
    auto* item = _list->currentItem();
    if (!item || !_doc) return;
    int index = item->data(Qt::UserRole).toInt();
    _doc->setActiveIndex(index);
    emit layerSelected(index);
}

void LayerPanel::onAddLayer() {
    if (!_doc) return;
    _doc->addRasterLayer();
    _doc->setActiveIndex(_doc->layerCount() - 1);
    rebuildList();
    emit layerAdded();
}

void LayerPanel::onDeleteLayer() {
    if (!_doc || _doc->layerCount() <= 1) return;
    auto* item = _list->currentItem();
    if (!item) return;
    int index = item->data(Qt::UserRole).toInt();
    _doc->removeLayer(index);
    int newActive = qMin(index, _doc->layerCount() - 1);
    _doc->setActiveIndex(newActive);
    rebuildList();
    emit layerRemoved(index);
}

void LayerPanel::onDuplicateLayer() {
    if (!_doc) return;
    auto* item = _list->currentItem();
    if (!item) return;
    int index = item->data(Qt::UserRole).toInt();
    _doc->duplicateLayer(index);
    rebuildList();
    emit layerAdded();
}

void LayerPanel::onMoveUp() {
    if (!_doc) return;
    auto* item = _list->currentItem();
    if (!item) return;
    int index = item->data(Qt::UserRole).toInt();
    if (index >= _doc->layerCount() - 1) return;
    _doc->moveLayer(index, index + 1);
    _doc->setActiveIndex(index + 1);
    rebuildList();
}

void LayerPanel::onMoveDown() {
    if (!_doc) return;
    auto* item = _list->currentItem();
    if (!item) return;
    int index = item->data(Qt::UserRole).toInt();
    if (index <= 0) return;
    _doc->moveLayer(index, index - 1);
    _doc->setActiveIndex(index - 1);
    rebuildList();
}

} // namespace dexilate::ui
