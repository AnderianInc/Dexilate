// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include <cstddef>
#include <cstdint>

namespace dexilate::engine {

class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo()    = 0;
    virtual size_t memoryCost() const noexcept { return 0; }
};

} // namespace dexilate::engine
