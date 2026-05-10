// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

#include "dexilate/engine/document/Layer.h"
#include "dexilate/core/ICodec.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace dexilate::engine {

class Document {
public:
    // Create a blank transparent-canvas document.
    Document(uint32_t width, uint32_t height, std::string title = "Untitled");

    // Create a document from a decoded image (one raster layer).
    static Document fromImage(const core::ImageData& img,
                               const std::filesystem::path& sourcePath = {});

    uint32_t width()  const noexcept { return _width; }
    uint32_t height() const noexcept { return _height; }

    const std::string& title()    const noexcept { return _title; }
    void               setTitle(std::string t)   { _title = std::move(t); }

    bool isModified() const noexcept { return _modified; }
    void markModified()              { _modified = true; }
    void clearModified()             { _modified = false; }

    // File path associated with the document (empty if never saved).
    const std::filesystem::path& filePath() const noexcept { return _filePath; }
    void setFilePath(std::filesystem::path p) { _filePath = std::move(p); }

    // Layer stack management
    int  layerCount()  const noexcept;
    int  activeIndex() const noexcept { return _activeIndex; }
    void setActiveIndex(int i);

    Layer*       layerAt(int i)       noexcept;
    const Layer* layerAt(int i) const noexcept;

    // Returns nullptr if the active layer is not a RasterLayer.
    RasterLayer* activeLayer() noexcept;
    const RasterLayer* activeLayer() const noexcept;

    // Returns the new layer's index.
    int addLayer(std::unique_ptr<Layer> layer);
    // Convenience: adds a blank raster layer and returns its index.
    int addRasterLayer(std::string name = "Layer");

    void removeLayer(int index);
    void moveLayer(int from, int to);
    void duplicateLayer(int index);

    // Flatten all visible layers into a single RGBA8 buffer (for save/export).
    std::vector<uint8_t> flatten() const;

private:
    uint32_t    _width, _height;
    std::string _title;
    bool        _modified = false;
    std::filesystem::path _filePath;

    std::vector<std::unique_ptr<Layer>> _layers;
    int _activeIndex = 0;
};

} // namespace dexilate::engine
