// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  tests/engine/test_history.cpp
//  Unit tests for HistoryManager (execute / undo / redo / memory budget).
// ─────────────────────────────────────────────────────────────────────────────

#include <catch2/catch_test_macros.hpp>

#include "dexilate/engine/document/HistoryManager.h"
#include "dexilate/engine/document/Command.h"

#include <memory>

using namespace dexilate::engine;

// ── CounterCommand ─────────────────────────────────────────────────────────────
// Increments a counter on execute(), decrements on undo().
// Reports a configurable memoryCost so we can test budget trimming.

class CounterCommand : public Command {
public:
    explicit CounterCommand(int& counter, size_t cost = 0)
        : _counter(counter), _cost(cost) {}

    void execute() override { ++_counter; }
    void undo()    override { --_counter; }
    size_t memoryCost() const noexcept override { return _cost; }

private:
    int&   _counter;
    size_t _cost;
};

// Convenience factory.
static std::unique_ptr<CounterCommand> makeCmd(int& c, size_t cost = 0) {
    return std::make_unique<CounterCommand>(c, cost);
}

// ── Fresh HistoryManager ───────────────────────────────────────────────────────

TEST_CASE("HistoryManager - fresh manager: canUndo/canRedo both false", "[history]") {
    HistoryManager hm;
    REQUIRE_FALSE(hm.canUndo());
    REQUIRE_FALSE(hm.canRedo());
    REQUIRE(hm.undoCount() == 0);
    REQUIRE(hm.redoCount() == 0);
}

// ── Execute ───────────────────────────────────────────────────────────────────

TEST_CASE("HistoryManager - execute: canUndo becomes true, canRedo stays false", "[history]") {
    HistoryManager hm;
    int counter = 0;
    hm.execute(makeCmd(counter));

    REQUIRE(counter == 1);
    REQUIRE(hm.canUndo());
    REQUIRE_FALSE(hm.canRedo());
    REQUIRE(hm.undoCount() == 1);
    REQUIRE(hm.redoCount() == 0);
}

TEST_CASE("HistoryManager - execute multiple commands accumulate on undo stack", "[history]") {
    HistoryManager hm;
    int counter = 0;
    hm.execute(makeCmd(counter));
    hm.execute(makeCmd(counter));
    hm.execute(makeCmd(counter));

    REQUIRE(counter == 3);
    REQUIRE(hm.undoCount() == 3);
    REQUIRE(hm.redoCount() == 0);
}

// ── Undo ──────────────────────────────────────────────────────────────────────

TEST_CASE("HistoryManager - undo: single command leaves canUndo false, canRedo true", "[history]") {
    HistoryManager hm;
    int counter = 0;
    hm.execute(makeCmd(counter));
    hm.undo();

    REQUIRE(counter == 0);           // command was reversed
    REQUIRE_FALSE(hm.canUndo());
    REQUIRE(hm.canRedo());
    REQUIRE(hm.undoCount() == 0);
    REQUIRE(hm.redoCount() == 1);
}

TEST_CASE("HistoryManager - undo on empty stack is a no-op", "[history]") {
    HistoryManager hm;
    int counter = 0;
    // Should not throw or crash
    hm.undo();
    REQUIRE(counter == 0);
    REQUIRE_FALSE(hm.canUndo());
    REQUIRE_FALSE(hm.canRedo());
}

TEST_CASE("HistoryManager - undo multiple steps in order", "[history]") {
    HistoryManager hm;
    int counter = 0;
    hm.execute(makeCmd(counter));  // counter = 1
    hm.execute(makeCmd(counter));  // counter = 2
    hm.undo();                     // counter = 1
    REQUIRE(counter == 1);
    REQUIRE(hm.undoCount() == 1);
    REQUIRE(hm.redoCount() == 1);
}

// ── Redo ──────────────────────────────────────────────────────────────────────

TEST_CASE("HistoryManager - redo: restores command, canRedo becomes false", "[history]") {
    HistoryManager hm;
    int counter = 0;
    hm.execute(makeCmd(counter));
    hm.undo();
    hm.redo();

    REQUIRE(counter == 1);          // re-executed
    REQUIRE(hm.canUndo());
    REQUIRE_FALSE(hm.canRedo());
    REQUIRE(hm.undoCount() == 1);
    REQUIRE(hm.redoCount() == 0);
}

TEST_CASE("HistoryManager - redo on empty redo stack is a no-op", "[history]") {
    HistoryManager hm;
    int counter = 0;
    hm.execute(makeCmd(counter));
    // No undo performed; redo stack is empty.
    hm.redo();
    REQUIRE(counter == 1);  // unchanged
}

// ── Execute clears redo stack ─────────────────────────────────────────────────

TEST_CASE("HistoryManager - execute after undo clears redo stack", "[history]") {
    HistoryManager hm;
    int counter = 0;
    hm.execute(makeCmd(counter));  // counter=1, undo=[A]
    hm.undo();                     // counter=0, redo=[A]
    REQUIRE(hm.canRedo());

    hm.execute(makeCmd(counter));  // counter=1, redo should be wiped
    REQUIRE(hm.canUndo());
    REQUIRE_FALSE(hm.canRedo());
    REQUIRE(hm.redoCount() == 0);
}

// ── Memory budget trimming ────────────────────────────────────────────────────

TEST_CASE("HistoryManager - large command trimmed when budget exceeded", "[history]") {
    // Budget: 100 bytes.  First command costs 60, second costs 60 → total 120 > 100.
    // trimToMemoryBudget() should drop the oldest entry.
    HistoryManager hm(100);
    int counter = 0;
    hm.execute(makeCmd(counter, 60));  // entry 1: cost 60, total=60
    hm.execute(makeCmd(counter, 60));  // entry 2: cost 60, total=120 → trim entry 1

    // The oldest command was trimmed; only one should remain.
    REQUIRE(hm.undoCount() == 1);
    REQUIRE(hm.canUndo());
}

TEST_CASE("HistoryManager - commands within budget are not trimmed", "[history]") {
    HistoryManager hm(200);
    int counter = 0;
    hm.execute(makeCmd(counter, 60));
    hm.execute(makeCmd(counter, 60));
    hm.execute(makeCmd(counter, 60));  // total 180 < 200

    REQUIRE(hm.undoCount() == 3);
}

TEST_CASE("HistoryManager - single command exceeding budget trims itself", "[history]") {
    // A command whose cost is larger than the entire budget; it is trimmed
    // immediately, leaving the undo stack empty.
    HistoryManager hm(50);
    int counter = 0;
    hm.execute(makeCmd(counter, 100));  // cost 100 > budget 50 → trimmed

    REQUIRE(hm.undoCount() == 0);
    REQUIRE_FALSE(hm.canUndo());
    // The command was still executed (counter incremented).
    REQUIRE(counter == 1);
}

// ── clear() ───────────────────────────────────────────────────────────────────

TEST_CASE("HistoryManager - clear resets both stacks and memory", "[history]") {
    HistoryManager hm;
    int counter = 0;
    hm.execute(makeCmd(counter));
    hm.execute(makeCmd(counter));
    hm.undo();

    REQUIRE(hm.canUndo());
    REQUIRE(hm.canRedo());

    hm.clear();

    REQUIRE_FALSE(hm.canUndo());
    REQUIRE_FALSE(hm.canRedo());
    REQUIRE(hm.undoCount() == 0);
    REQUIRE(hm.redoCount() == 0);
}

TEST_CASE("HistoryManager - execute after clear works normally", "[history]") {
    HistoryManager hm;
    int counter = 0;
    hm.execute(makeCmd(counter));
    hm.clear();
    hm.execute(makeCmd(counter));

    REQUIRE(hm.undoCount() == 1);
    REQUIRE(hm.canUndo());
    REQUIRE_FALSE(hm.canRedo());
}
