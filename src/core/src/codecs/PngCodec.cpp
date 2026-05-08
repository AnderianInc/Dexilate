// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#include "dexilate/core/codecs/PngCodec.h"

#include <png.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace dexilate::core {

// ── Decode ────────────────────────────────────────────────────────────────────
ImageData PngCodec::decode(const std::filesystem::path& path) {
    std::string pathStr = path.string();
    FILE* fp = std::fopen(pathStr.c_str(), "rb");
    if (!fp)
        throw CodecError("PngCodec: cannot open '" + pathStr + "'");

    // Verify PNG signature
    uint8_t sig[8];
    if (std::fread(sig, 1, 8, fp) != 8 || png_sig_cmp(sig, 0, 8)) {
        std::fclose(fp);
        throw CodecError("PngCodec: not a PNG file: " + pathStr);
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                             nullptr, nullptr, nullptr);
    if (!png) { std::fclose(fp); throw CodecError("PngCodec: png_create_read_struct failed"); }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        std::fclose(fp);
        throw CodecError("PngCodec: png_create_info_struct failed");
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(fp);
        throw CodecError("PngCodec: error reading " + pathStr);
    }

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);  // already consumed the signature above
    png_read_info(png, info);

    uint32_t w    = png_get_image_width(png, info);
    uint32_t h    = png_get_image_height(png, info);
    int colorType = png_get_color_type(png, info);
    int bitDepth  = png_get_bit_depth(png, info);

    // Normalise any colour type to RGBA8
    if (bitDepth == 16)
        png_set_strip_16(png);
    if (colorType == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    if (colorType == PNG_COLOR_TYPE_RGB  ||
        colorType == PNG_COLOR_TYPE_GRAY ||
        colorType == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (colorType == PNG_COLOR_TYPE_GRAY ||
        colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    ImageData img;
    img.width  = w;
    img.height = h;
    img.pixels.resize(static_cast<size_t>(w) * h * 4);

    std::vector<png_bytep> rows(h);
    for (uint32_t y = 0; y < h; ++y)
        rows[y] = img.pixels.data() + y * w * 4;

    png_read_image(png, rows.data());
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(fp);
    return img;
}

// ── Encode ────────────────────────────────────────────────────────────────────
void PngCodec::encode(const std::filesystem::path& path,
                      uint32_t width, uint32_t height,
                      const uint8_t* rgba8) {
    std::string pathStr = path.string();
    FILE* fp = std::fopen(pathStr.c_str(), "wb");
    if (!fp)
        throw CodecError("PngCodec: cannot open for write: " + pathStr);

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              nullptr, nullptr, nullptr);
    if (!png) { std::fclose(fp); throw CodecError("PngCodec: png_create_write_struct failed"); }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        std::fclose(fp);
        throw CodecError("PngCodec: png_create_info_struct failed");
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        std::fclose(fp);
        throw CodecError("PngCodec: error writing " + pathStr);
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info,
                 width, height, 8,
                 PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    std::vector<png_bytep> rows(height);
    for (uint32_t y = 0; y < height; ++y)
        rows[y] = const_cast<png_bytep>(rgba8 + y * width * 4);

    png_write_image(png, rows.data());
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(fp);
}

} // namespace dexilate::core
