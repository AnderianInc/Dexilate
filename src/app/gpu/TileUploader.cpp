// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "TileUploader.h"
#include "dexilate/engine/document/Layer.h"
#include "dexilate/core/Tile.h"

#include <cstring>
#include <stdexcept>
#include <vector>

namespace dexilate {

TileUploader::TileUploader(WGPUDevice device, WGPUQueue queue,
                            uint32_t docWidth, uint32_t docHeight)
    : _device(device), _queue(queue)
    , _width(docWidth), _height(docHeight)
{
    WGPUTextureDescriptor desc = {};
    desc.usage         = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    desc.dimension     = WGPUTextureDimension_2D;
    desc.size          = { docWidth, docHeight, 1 };
    desc.format        = WGPUTextureFormat_RGBA8Unorm;
    desc.mipLevelCount = 1;
    desc.sampleCount   = 1;
    _texture = wgpuDeviceCreateTexture(_device, &desc);
    if (!_texture)
        throw std::runtime_error("TileUploader: wgpuDeviceCreateTexture failed");

    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.format          = WGPUTextureFormat_RGBA8Unorm;
    viewDesc.dimension       = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount   = 1;
    viewDesc.arrayLayerCount = 1;
    _view = wgpuTextureCreateView(_texture, &viewDesc);
}

TileUploader::~TileUploader() {
    if (_view)    wgpuTextureViewRelease(_view);
    if (_texture) wgpuTextureRelease(_texture);
}

void TileUploader::uploadDirtyTiles(engine::RasterLayer& layer) {
    auto& tileManager = layer.tiles();
    auto dirty = tileManager.dirtyIndices();
    if (dirty.empty()) return;

    int tilesX = tileManager.tilesX();
    int ts     = core::Tile::SIZE;

    for (int idx : dirty) {
        int tx = idx % tilesX;
        int ty = idx / tilesX;

        const core::Tile* tile = tileManager.at(tx, ty);
        if (!tile) continue;

        // Clamp tile upload rect to document bounds
        uint32_t originX = static_cast<uint32_t>(tx * ts);
        uint32_t originY = static_cast<uint32_t>(ty * ts);
        uint32_t copyW   = std::min(static_cast<uint32_t>(ts), _width  - originX);
        uint32_t copyH   = std::min(static_cast<uint32_t>(ts), _height - originY);

        WGPUImageCopyTexture dst = {};
        dst.texture  = _texture;
        dst.mipLevel = 0;
        dst.origin   = { originX, originY, 0 };
        dst.aspect   = WGPUTextureAspect_All;

        WGPUTextureDataLayout layout = {};
        layout.offset       = 0;
        layout.bytesPerRow  = static_cast<uint32_t>(ts) * 4;
        layout.rowsPerImage = static_cast<uint32_t>(ts);

        WGPUExtent3D extent = { copyW, copyH, 1 };

        wgpuQueueWriteTexture(_queue, &dst, tile->data(),
                              core::Tile::BYTES, &layout, &extent);
    }

    tileManager.clearAllDirty();
}

} // namespace dexilate
