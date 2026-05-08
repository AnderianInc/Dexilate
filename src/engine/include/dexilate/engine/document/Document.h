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

// Single-layer raster document for Phase 1.
// Phase 2 will extend this to a heterogeneous layer tree.
class Document {
public:
    // Create a blank white-canvas document (all pixels transparent).
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

    // Layer access — Phase 1 always has exactly one raster layer.
    RasterLayer*       activeLayer()       noexcept;
    const RasterLayer* activeLayer() const noexcept;

    int  layerCount() const noexcept { return static_cast<int>(_layers.size()); }

    // File path associated with the document (empty if never saved).
    const std::filesystem::path& filePath() const noexcept { return _filePath; }
    void setFilePath(std::filesystem::path p) { _filePath = std::move(p); }

private:
    uint32_t    _width, _height;
    std::string _title;
    bool        _modified = false;
    std::filesystem::path _filePath;

    std::vector<std::unique_ptr<Layer>> _layers;
    int _activeIndex = 0;
};

} // namespace dexilate::engine
