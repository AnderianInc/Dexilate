// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "Command.h"

#include <memory>
#include <vector>
#include <cstddef>

namespace dexilate::engine {

class HistoryManager {
public:
    // maxMemoryBytes: total memory budget for stored commands (default 256 MB).
    explicit HistoryManager(size_t maxMemoryBytes = 256 * 1024 * 1024);

    void execute(std::unique_ptr<Command> cmd);
    bool canUndo() const noexcept;
    bool canRedo() const noexcept;
    void undo();
    void redo();
    void clear();

    int undoCount() const noexcept;
    int redoCount() const noexcept;

private:
    void trimToMemoryBudget();

    std::vector<std::unique_ptr<Command>> _undoStack;
    std::vector<std::unique_ptr<Command>> _redoStack;
    size_t _maxMemory;
    size_t _usedMemory = 0;
};

} // namespace dexilate::engine
