// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/platform/src/macos/MacClipboard.mm
//  macOS implementation of IClipboard
//
//  Image write: RGBA8 straight-alpha → CGImage → NSImage → pasteboard (TIFF).
//  Image read:  pasteboard → NSImage → CGBitmapContext → RGBA8 premultiplied.
//               Note: CGBitmapContext normalises to premultiplied alpha; callers
//               that need straight alpha must un-premultiply themselves.
//
//  Text:        NSPasteboardTypeString (UTF-8 encoded NSString).
//
//  SVG path:    Dual-format write —
//               "com.dexilate.svg-path" (custom UTI, Dexilate-to-Dexilate)
//               + NSPasteboardTypeString (plain SVG markup, external app interop).
//               Read prefers the custom UTI; falls back to plain string.
//
//  All NSPasteboard operations must occur on the main thread.
// ─────────────────────────────────────────────────────────────────────────────

#include "dexilate/platform/IClipboard.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#include <CoreGraphics/CoreGraphics.h>

namespace dexilate::platform {

namespace {

// Dexilate-specific UTI for lossless SVG path round-trips between Dexilate instances.
// Also registered in the app's Info.plist under UTExportedTypeDeclarations.
NSString* const k_svgPathType = @"com.dexilate.svg-path";

// Build a CGImage from RGBA8 straight-alpha pixel data.
// Caller must CGImageRelease the returned image.
CGImageRef cgImageFromRGBA8(const uint8_t* pixels, uint32_t w, uint32_t h) {
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    size_t rowBytes = static_cast<size_t>(w) * 4;
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, pixels, rowBytes * h);
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
    CFRelease(data);
    CGImageRef img = CGImageCreate(
        w, h, 8, 32, rowBytes, cs,
        kCGBitmapByteOrderDefault | kCGImageAlphaLast,  // straight alpha
        provider, nullptr, false, kCGRenderingIntentDefault
    );
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(cs);
    return img;
}

// Decode NSImage to RGBA8 premultiplied-alpha via CGBitmapContext.
std::optional<ClipboardImage> decodeNSImage(NSImage* nsImg) {
    if (!nsImg) return std::nullopt;
    CGImageRef cg = [nsImg CGImageForProposedRect:nil context:nil hints:nil];
    if (!cg) return std::nullopt;

    size_t w = CGImageGetWidth(cg);
    size_t h = CGImageGetHeight(cg);
    if (w == 0 || h == 0) return std::nullopt;

    ClipboardImage out;
    out.width  = static_cast<uint32_t>(w);
    out.height = static_cast<uint32_t>(h);
    out.pixels.resize(w * h * 4, 0);

    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef ctx = CGBitmapContextCreate(
        out.pixels.data(), w, h, 8, w * 4, cs,
        kCGBitmapByteOrderDefault | kCGImageAlphaPremultipliedLast
    );
    if (ctx) {
        CGContextDrawImage(ctx,
            CGRectMake(0, 0, static_cast<CGFloat>(w), static_cast<CGFloat>(h)), cg);
        CGContextRelease(ctx);
    }
    CGColorSpaceRelease(cs);
    if (!ctx) return std::nullopt;
    return out;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
class MacClipboard final : public IClipboard {
public:
    // ── Availability ──────────────────────────────────────────────────────────
    bool hasImage() const override {
        @autoreleasepool {
            return [[NSPasteboard generalPasteboard]
                canReadObjectForClasses:@[[NSImage class]] options:nil];
        }
    }

    bool hasText() const override {
        @autoreleasepool {
            return [[NSPasteboard generalPasteboard]
                availableTypeFromArray:@[NSPasteboardTypeString]] != nil;
        }
    }

    bool hasSvgPath() const override {
        @autoreleasepool {
            return [[NSPasteboard generalPasteboard]
                availableTypeFromArray:@[k_svgPathType]] != nil;
        }
    }

    // ── Write ─────────────────────────────────────────────────────────────────
    void setImage(const ClipboardImage& image) override {
        @autoreleasepool {
            CGImageRef cg = cgImageFromRGBA8(
                image.pixels.data(), image.width, image.height);
            NSImage* nsImg = [[NSImage alloc] initWithCGImage:cg
                size:NSMakeSize(image.width, image.height)];
            CGImageRelease(cg);
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            [pb clearContents];
            [pb writeObjects:@[nsImg]];
        }
    }

    void setText(const std::string& text) override {
        @autoreleasepool {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            [pb clearContents];
            [pb declareTypes:@[NSPasteboardTypeString] owner:nil];
            [pb setString:[NSString stringWithUTF8String:text.c_str()]
                  forType:NSPasteboardTypeString];
        }
    }

    void setSvgPath(const std::string& svgPathData) override {
        @autoreleasepool {
            NSString* nsSvg = [NSString stringWithUTF8String:svgPathData.c_str()];
            NSData*   raw   = [nsSvg dataUsingEncoding:NSUTF8StringEncoding];
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            [pb clearContents];
            // Write custom UTI for Dexilate-to-Dexilate lossless round-trips,
            // plus plain string so Inkscape / Affinity Designer can paste.
            [pb declareTypes:@[k_svgPathType, NSPasteboardTypeString] owner:nil];
            [pb setData:raw forType:k_svgPathType];
            [pb setString:nsSvg forType:NSPasteboardTypeString];
        }
    }

    // ── Read ──────────────────────────────────────────────────────────────────
    std::optional<ClipboardImage> getImage() const override {
        @autoreleasepool {
            NSArray* imgs = [[NSPasteboard generalPasteboard]
                readObjectsForClasses:@[[NSImage class]] options:nil];
            if (!imgs || imgs.count == 0) return std::nullopt;
            return decodeNSImage(imgs.firstObject);
        }
    }

    std::optional<std::string> getText() const override {
        @autoreleasepool {
            NSString* s = [[NSPasteboard generalPasteboard]
                stringForType:NSPasteboardTypeString];
            if (!s) return std::nullopt;
            return std::string{s.UTF8String};
        }
    }

    std::optional<std::string> getSvgPath() const override {
        @autoreleasepool {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            // Prefer custom UTI (Dexilate-to-Dexilate, lossless)
            NSData* raw = [pb dataForType:k_svgPathType];
            if (raw) {
                NSString* s = [[NSString alloc] initWithData:raw
                    encoding:NSUTF8StringEncoding];
                if (s) return std::string{s.UTF8String};
            }
            // Fall back to plain SVG text placed by external apps
            NSString* text = [pb stringForType:NSPasteboardTypeString];
            if (text) return std::string{text.UTF8String};
            return std::nullopt;
        }
    }

    // ── Clear ─────────────────────────────────────────────────────────────────
    void clear() override {
        @autoreleasepool {
            [[NSPasteboard generalPasteboard] clearContents];
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<IClipboard> IClipboard::create() {
    return std::make_unique<MacClipboard>();
}

} // namespace dexilate::platform
