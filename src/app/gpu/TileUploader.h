// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include <webgpu/wgpu.h>
#include <cstdint>
#include <unordered_map>

namespace dexilate::engine { class RasterLayer; }

namespace dexilate {

// Manages a single wgpu texture that mirrors the active raster layer.
// On each frame: upload dirty tile regions, return the texture for binding.
//
// Phase 1: one texture covers the entire document (not per-tile textures).
// Dirty tiles are written via wgpuQueueWriteTexture so only changed regions
// transit the PCIe bus.
class TileUploader {
public:
    TileUploader(WGPUDevice device, WGPUQueue queue,
                 uint32_t docWidth, uint32_t docHeight);
    ~TileUploader();

    TileUploader(const TileUploader&)            = delete;
    TileUploader& operator=(const TileUploader&) = delete;

    // Upload all dirty tiles from the layer and clear their dirty flag.
    void uploadDirtyTiles(engine::RasterLayer& layer);

    // The full-document RGBA8 texture (valid after at least one upload).
    WGPUTexture     texture()     const noexcept { return _texture; }
    WGPUTextureView textureView() const noexcept { return _view; }

private:
    WGPUDevice      _device  = nullptr;
    WGPUQueue       _queue   = nullptr;
    WGPUTexture     _texture = nullptr;
    WGPUTextureView _view    = nullptr;
    uint32_t        _width   = 0;
    uint32_t        _height  = 0;
};

} // namespace dexilate
