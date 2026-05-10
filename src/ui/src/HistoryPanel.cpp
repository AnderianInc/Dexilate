// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "HistoryPanel.h"
#include "dexilate/engine/document/HistoryManager.h"

#include <QVBoxLayout>
#include <QWidget>

namespace dexilate::ui {

HistoryPanel::HistoryPanel(QWidget* parent)
    : QDockWidget("History", parent)
{
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QWidget* container  = new QWidget(this);
    QVBoxLayout* vlay   = new QVBoxLayout(container);
    vlay->setContentsMargins(4, 4, 4, 4);

    _list = new QListWidget(container);
    _list->setSelectionMode(QAbstractItemView::NoSelection);
    vlay->addWidget(_list);

    setWidget(container);
}

void HistoryPanel::setHistoryManager(engine::HistoryManager* history) {
    _history = history;
    refresh();
}

void HistoryPanel::refresh() {
    _list->clear();
    if (!_history) return;

    int undos = _history->undoCount();
    int redos = _history->redoCount();

    for (int i = 0; i < undos; ++i) {
        auto* item = new QListWidgetItem(QStringLiteral("Step %1").arg(i + 1), _list);
        (void)item;
    }

    // Greyed-out entries for the redo stack
    for (int i = 0; i < redos; ++i) {
        auto* item = new QListWidgetItem(QStringLiteral("Step %1 (redo)").arg(undos + i + 1), _list);
        item->setForeground(Qt::gray);
    }
}

} // namespace dexilate::ui
