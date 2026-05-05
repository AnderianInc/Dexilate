// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  src/app/GpuContext.h
//
//  RAII wrapper around the three wgpu-native objects required to render:
//    WGPUAdapter — the physical GPU chosen for the target surface
//    WGPUDevice  — the logical device (command queues, resource allocation)
//    WGPUQueue   — the default command submission queue
//
//  Usage:
//    WGPUInstance instance = wgpuCreateInstance(nullptr);
//    WGPUSurface  surface  = ...;          // platform surface (see main.cpp)
//    dexilate::GpuContext gpu = dexilate::GpuContext::create(instance, surface);
//    // use gpu.device() / gpu.queue() for rendering
//
//  Thread safety: all methods must be called from the main thread.
// ─────────────────────────────────────────────────────────────────────────────

#include <webgpu/wgpu.h>

namespace dexilate {

class GpuContext {
public:
    /// Requests the best adapter compatible with `surface`, then creates a
    /// device with default limits and features. Throws on failure.
    [[nodiscard]] static GpuContext create(WGPUInstance instance,
                                           WGPUSurface  surface);

    ~GpuContext();

    GpuContext(const GpuContext&)            = delete;
    GpuContext& operator=(const GpuContext&) = delete;
    GpuContext(GpuContext&&) noexcept;
    GpuContext& operator=(GpuContext&&) noexcept;

    [[nodiscard]] WGPUAdapter adapter() const noexcept { return _adapter; }
    [[nodiscard]] WGPUDevice  device()  const noexcept { return _device;  }
    [[nodiscard]] WGPUQueue   queue()   const noexcept { return _queue;   }

private:
    GpuContext() = default;
    WGPUAdapter _adapter = nullptr;
    WGPUDevice  _device  = nullptr;
    WGPUQueue   _queue   = nullptr;
};

} // namespace dexilate
