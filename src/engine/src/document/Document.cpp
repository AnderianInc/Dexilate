// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/engine/document/Document.h"
#include "dexilate/engine/raster/Compositor.h"

#include <algorithm>
#include <stdexcept>

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

    for (uint32_t y = 0; y < img.height; ++y) {
        for (uint32_t x = 0; x < img.width; ++x) {
            const uint8_t* px = img.pixels.data() + (y * img.width + x) * 4;
            layer->paintPixel(static_cast<int>(x), static_cast<int>(y),
                              px[0], px[1], px[2], px[3]);
        }
    }
    layer->clearDirty();
    return doc;
}

int Document::layerCount() const noexcept {
    return static_cast<int>(_layers.size());
}

void Document::setActiveIndex(int i) {
    if (i < 0 || i >= static_cast<int>(_layers.size())) return;
    _activeIndex = i;
}

Layer* Document::layerAt(int i) noexcept {
    if (i < 0 || i >= static_cast<int>(_layers.size())) return nullptr;
    return _layers[static_cast<size_t>(i)].get();
}

const Layer* Document::layerAt(int i) const noexcept {
    if (i < 0 || i >= static_cast<int>(_layers.size())) return nullptr;
    return _layers[static_cast<size_t>(i)].get();
}

RasterLayer* Document::activeLayer() noexcept {
    if (_layers.empty()) return nullptr;
    return dynamic_cast<RasterLayer*>(_layers[static_cast<size_t>(_activeIndex)].get());
}

const RasterLayer* Document::activeLayer() const noexcept {
    if (_layers.empty()) return nullptr;
    return dynamic_cast<const RasterLayer*>(_layers[static_cast<size_t>(_activeIndex)].get());
}

int Document::addLayer(std::unique_ptr<Layer> layer) {
    _layers.push_back(std::move(layer));
    return static_cast<int>(_layers.size()) - 1;
}

int Document::addRasterLayer(std::string name) {
    auto layer = std::make_unique<RasterLayer>(_width, _height, std::move(name));
    return addLayer(std::move(layer));
}

void Document::removeLayer(int index) {
    if (index < 0 || index >= static_cast<int>(_layers.size())) return;
    _layers.erase(_layers.begin() + index);
    // Keep _activeIndex in bounds.
    if (_activeIndex >= static_cast<int>(_layers.size()))
        _activeIndex = std::max(0, static_cast<int>(_layers.size()) - 1);
}

void Document::moveLayer(int from, int to) {
    int n = static_cast<int>(_layers.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) return;

    auto layer = std::move(_layers[static_cast<size_t>(from)]);
    _layers.erase(_layers.begin() + from);
    _layers.insert(_layers.begin() + to, std::move(layer));

    // Update active index to follow the moved layer if it was the active one.
    if (_activeIndex == from) {
        _activeIndex = to;
    } else {
        // Adjust for the shift caused by the move.
        if (from < to && _activeIndex > from && _activeIndex <= to)
            --_activeIndex;
        else if (from > to && _activeIndex >= to && _activeIndex < from)
            ++_activeIndex;
    }
}

void Document::duplicateLayer(int index) {
    if (index < 0 || index >= static_cast<int>(_layers.size())) return;

    const Layer* src = _layers[static_cast<size_t>(index)].get();

    if (const auto* raster = dynamic_cast<const RasterLayer*>(src)) {
        auto dup = std::make_unique<RasterLayer>(_width, _height, raster->name + " copy");
        dup->visible   = raster->visible;
        dup->locked    = raster->locked;
        dup->opacity   = raster->opacity;
        dup->blendMode = raster->blendMode;

        // Copy pixel data tile by tile.
        for (uint32_t y = 0; y < _height; ++y) {
            for (uint32_t x = 0; x < _width; ++x) {
                int tx = raster->tiles().tileX(static_cast<int>(x));
                int ty = raster->tiles().tileY(static_cast<int>(y));
                const core::Tile* tile = raster->tiles().at(tx, ty);
                if (!tile) continue;
                int lx = static_cast<int>(x) % core::Tile::SIZE;
                int ly = static_cast<int>(y) % core::Tile::SIZE;
                const uint8_t* px = tile->pixelAt(lx, ly);
                dup->paintPixel(static_cast<int>(x), static_cast<int>(y),
                                px[0], px[1], px[2], px[3]);
            }
        }
        dup->clearDirty();
        _layers.insert(_layers.begin() + index + 1, std::move(dup));
    }
    // GroupLayer and AdjustmentLayer duplication can be added in later phases.
}

std::vector<uint8_t> Document::flatten() const {
    std::vector<uint8_t> out(static_cast<size_t>(_width) * _height * 4, 0);
    Compositor::composite(*this, out.data(), _width, _height);
    return out;
}

} // namespace dexilate::engine
