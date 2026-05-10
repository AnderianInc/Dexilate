// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include <QDockWidget>
#include <QListWidget>

namespace dexilate::engine { class HistoryManager; }

namespace dexilate::ui {

class HistoryPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit HistoryPanel(QWidget* parent = nullptr);

    void setHistoryManager(engine::HistoryManager* history);
    void refresh();

private:
    engine::HistoryManager* _history = nullptr;
    QListWidget*            _list    = nullptr;
};

} // namespace dexilate::ui
