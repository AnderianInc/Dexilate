// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/engine/document/HistoryManager.h"

namespace dexilate::engine {

HistoryManager::HistoryManager(size_t maxMemoryBytes)
    : _maxMemory(maxMemoryBytes)
{}

void HistoryManager::execute(std::unique_ptr<Command> cmd) {
    cmd->execute();

    _usedMemory += cmd->memoryCost();

    // Any branching redo history is discarded on a new action.
    for (const auto& c : _redoStack)
        _usedMemory -= c->memoryCost();
    _redoStack.clear();

    _undoStack.push_back(std::move(cmd));
    trimToMemoryBudget();
}

bool HistoryManager::canUndo() const noexcept {
    return !_undoStack.empty();
}

bool HistoryManager::canRedo() const noexcept {
    return !_redoStack.empty();
}

void HistoryManager::undo() {
    if (_undoStack.empty()) return;
    auto& cmd = _undoStack.back();
    cmd->undo();
    _usedMemory -= cmd->memoryCost();
    _redoStack.push_back(std::move(cmd));
    _undoStack.pop_back();
    _usedMemory += _redoStack.back()->memoryCost();
}

void HistoryManager::redo() {
    if (_redoStack.empty()) return;
    auto& cmd = _redoStack.back();
    cmd->execute();
    _usedMemory -= cmd->memoryCost();
    _undoStack.push_back(std::move(cmd));
    _redoStack.pop_back();
    _usedMemory += _undoStack.back()->memoryCost();
}

void HistoryManager::clear() {
    _undoStack.clear();
    _redoStack.clear();
    _usedMemory = 0;
}

int HistoryManager::undoCount() const noexcept {
    return static_cast<int>(_undoStack.size());
}

int HistoryManager::redoCount() const noexcept {
    return static_cast<int>(_redoStack.size());
}

void HistoryManager::trimToMemoryBudget() {
    // Drop oldest undo entries from the front until within budget.
    while (_usedMemory > _maxMemory && !_undoStack.empty()) {
        _usedMemory -= _undoStack.front()->memoryCost();
        _undoStack.erase(_undoStack.begin());
    }
}

} // namespace dexilate::engine
