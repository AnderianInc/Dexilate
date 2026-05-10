// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

namespace dexilate::engine {

// Abstract interface for interactive tools that respond to pointer events.
class ITool {
public:
    virtual ~ITool() = default;

    virtual void onPress(float x, float y, float pressure) = 0;
    virtual void onMove(float x, float y, float pressure)  = 0;
    virtual void onRelease(float x, float y)               = 0;
};

} // namespace dexilate::engine
