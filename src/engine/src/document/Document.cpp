// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/engine/document/Document.h"

namespace dexilate::engine {

Document::Document(uint32_t width, uint32_t height, std::string title)
    : _width(width), _height(height), _title(std::move(title))
{
    _layers.push_back(std::make_unique<RasterLayer>(width, height, "Layer 1"));
}

Document Document::fromImage(const core::ImageData& img,
                              const std::filesystem::path& sourcePath) {
    Document doc(img.width, img.height,
                 sourcePath.empty() ? "Untitled"
                                    : sourcePath.stem().string());
    if (!sourcePath.empty())
        doc._filePath = sourcePath;

    auto* layer = doc.activeLayer();
    if (!layer) return doc;

    // Copy decoded pixels into the layer tiles
    for (uint32_t y = 0; y < img.height; ++y) {
        for (uint32_t x = 0; x < img.width; ++x) {
            const uint8_t* px = img.pixels.data() + (y * img.width + x) * 4;
            layer->paintPixel(static_cast<int>(x), static_cast<int>(y),
                              px[0], px[1], px[2], px[3]);
        }
    }
    layer->clearDirty();  // just loaded — nothing is "changed" yet
    return doc;
}

RasterLayer* Document::activeLayer() noexcept {
    if (_layers.empty()) return nullptr;
    return dynamic_cast<RasterLayer*>(_layers[static_cast<size_t>(_activeIndex)].get());
}

const RasterLayer* Document::activeLayer() const noexcept {
    if (_layers.empty()) return nullptr;
    return dynamic_cast<const RasterLayer*>(_layers[static_cast<size_t>(_activeIndex)].get());
}

} // namespace dexilate::engine
