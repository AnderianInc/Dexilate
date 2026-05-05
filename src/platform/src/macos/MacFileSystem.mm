// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/platform/src/macos/MacFileSystem.mm
//  macOS implementation of IFileSystem
//
//  Dialogs:     NSOpenPanel / NSSavePanel with UTType filters (macOS 12+),
//               falling back to the deprecated allowedFileTypes on macOS 11.
//  File I/O:    std::fstream — worker-thread safe; no ObjC ARC involved.
//  Atomic save: write to a sibling ".dexilate_tmp" file, then rename(2).
//               POSIX rename is atomic on APFS and HFS+ for same-directory ops.
//  Directories: NSSearchPathForDirectoriesInDomains / NSTemporaryDirectory.
//  Recent files: NSUserDefaults key "DexilateRecentFiles", max 20 entries,
//               stored as array of absolute path strings, MRU order.
// ─────────────────────────────────────────────────────────────────────────────

#include "dexilate/platform/IFileSystem.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <fstream>
#include <stdexcept>
#include <system_error>

namespace dexilate::platform {

// ─────────────────────────────────────────────────────────────────────────────
namespace {

constexpr size_t k_maxRecentFiles = 20;
NSString* const  k_recentFilesKey = @"DexilateRecentFiles";

// Extract the extension from a glob pattern like "*.canvas" → "canvas".
std::string extensionFromPattern(const std::string& pat) {
    if (pat.size() > 2 && pat[0] == '*' && pat[1] == '.')
        return pat.substr(2);
    return {};
}

API_AVAILABLE(macos(12.0))
NSArray<UTType*>* buildUTTypes(const std::vector<FileFilter>& filters) {
    if (filters.empty()) return nil;
    NSMutableArray<UTType*>* arr = [NSMutableArray array];
    for (const auto& f : filters) {
        for (const auto& p : f.patterns) {
            std::string ext = extensionFromPattern(p);
            if (ext.empty()) continue;
            UTType* t = [UTType typeWithFilenameExtension:
                             [NSString stringWithUTF8String:ext.c_str()]];
            if (t) [arr addObject:t];
        }
    }
    return arr.count ? [arr copy] : nil;
}

NSArray<NSString*>* buildExtensions(const std::vector<FileFilter>& filters) {
    NSMutableArray<NSString*>* arr = [NSMutableArray array];
    for (const auto& f : filters) {
        for (const auto& p : f.patterns) {
            std::string ext = extensionFromPattern(p);
            if (!ext.empty())
                [arr addObject:[NSString stringWithUTF8String:ext.c_str()]];
        }
    }
    return arr.count ? [arr copy] : nil;
}

NSURL* urlFromPath(const std::filesystem::path& p) {
    return [NSURL fileURLWithPath:[NSString stringWithUTF8String:p.c_str()]];
}

std::filesystem::path pathFromURL(NSURL* url) {
    return {[url.path UTF8String]};
}

void applyFilters(NSOpenPanel* panel, const std::vector<FileFilter>& filters) {
    if (@available(macOS 12.0, *)) {
        NSArray<UTType*>* types = buildUTTypes(filters);
        panel.allowedContentTypes = types ? types : @[];
    } else {
        NSArray<NSString*>* exts = buildExtensions(filters);
        if (exts) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            panel.allowedFileTypes = exts;
#pragma clang diagnostic pop
        }
    }
}

void applyFilters(NSSavePanel* panel, const std::vector<FileFilter>& filters) {
    if (@available(macOS 12.0, *)) {
        NSArray<UTType*>* types = buildUTTypes(filters);
        if (types) panel.allowedContentTypes = types;
    } else {
        NSArray<NSString*>* exts = buildExtensions(filters);
        if (exts) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            panel.allowedFileTypes = exts;
#pragma clang diagnostic pop
        }
    }
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
class MacFileSystem final : public IFileSystem {
public:
    // ── Dialogs ───────────────────────────────────────────────────────────────
    std::vector<std::filesystem::path>
    showOpenDialog(const OpenDialogOptions& opts) override {
        @autoreleasepool {
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            panel.title = [NSString stringWithUTF8String:opts.title.c_str()];
            panel.allowsMultipleSelection = opts.allowMultiple;
            panel.canChooseFiles = YES;
            panel.canChooseDirectories = NO;
            if (!opts.initialDirectory.empty())
                panel.directoryURL = urlFromPath(opts.initialDirectory);
            applyFilters(panel, opts.filters);

            std::vector<std::filesystem::path> result;
            if ([panel runModal] == NSModalResponseOK)
                for (NSURL* url in panel.URLs)
                    result.push_back(pathFromURL(url));
            return result;
        }
    }

    std::optional<std::filesystem::path>
    showSaveDialog(const SaveDialogOptions& opts) override {
        @autoreleasepool {
            NSSavePanel* panel = [NSSavePanel savePanel];
            panel.title = [NSString stringWithUTF8String:opts.title.c_str()];
            if (!opts.defaultFilename.empty())
                panel.nameFieldStringValue =
                    [NSString stringWithUTF8String:opts.defaultFilename.c_str()];
            if (!opts.initialDirectory.empty())
                panel.directoryURL = urlFromPath(opts.initialDirectory);
            applyFilters(panel, opts.filters);

            if ([panel runModal] == NSModalResponseOK)
                return pathFromURL(panel.URL);
            return std::nullopt;
        }
    }

    // ── File I/O ──────────────────────────────────────────────────────────────
    std::vector<uint8_t> readFile(const std::filesystem::path& path) override {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f)
            throw std::system_error(errno, std::generic_category(), path.string());
        auto size = f.tellg();
        f.seekg(0);
        std::vector<uint8_t> buf(static_cast<size_t>(size));
        if (!f.read(reinterpret_cast<char*>(buf.data()), size))
            throw std::system_error(errno, std::generic_category(), path.string());
        return buf;
    }

    void writeFileAtomic(const std::filesystem::path& path,
                         const uint8_t*               data,
                         size_t                        size) override {
        auto tmp = path;
        tmp += ".dexilate_tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f)
                throw std::system_error(errno, std::generic_category(), tmp.string());
            f.write(reinterpret_cast<const char*>(data),
                    static_cast<std::streamsize>(size));
            if (!f)
                throw std::system_error(errno, std::generic_category(), tmp.string());
        }
        // POSIX rename(2) is atomic for same-fs moves on APFS/HFS+.
        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            std::filesystem::remove(tmp);
            throw std::system_error(ec, path.string());
        }
    }

    // ── Directories ───────────────────────────────────────────────────────────
    std::filesystem::path tempDirectory() const override {
        return {NSTemporaryDirectory().UTF8String};
    }

    std::filesystem::path documentsDirectory() const override {
        @autoreleasepool {
            auto dirs = NSSearchPathForDirectoriesInDomains(
                NSDocumentDirectory, NSUserDomainMask, YES);
            if (dirs.count > 0)
                return {[dirs.firstObject UTF8String]};
            return std::filesystem::path{NSHomeDirectory().UTF8String} / "Documents";
        }
    }

    std::filesystem::path appDataDirectory() const override {
        @autoreleasepool {
            auto dirs = NSSearchPathForDirectoriesInDomains(
                NSApplicationSupportDirectory, NSUserDomainMask, YES);
            std::filesystem::path base;
            if (dirs.count > 0)
                base = std::filesystem::path([dirs.firstObject UTF8String]);
            else
                base = std::filesystem::path{NSHomeDirectory().UTF8String}
                       / "Library" / "Application Support";
            auto appDir = base / "Dexilate";
            std::filesystem::create_directories(appDir);
            return appDir;
        }
    }

    // ── Recent files ──────────────────────────────────────────────────────────
    std::vector<std::filesystem::path> recentFiles() const override {
        @autoreleasepool {
            NSArray<NSString*>* stored =
                [[NSUserDefaults standardUserDefaults] stringArrayForKey:k_recentFilesKey];
            std::vector<std::filesystem::path> out;
            if (!stored) return out;
            out.reserve(stored.count);
            for (NSString* s in stored)
                out.emplace_back(s.UTF8String);
            return out;
        }
    }

    void addRecentFile(const std::filesystem::path& path) override {
        @autoreleasepool {
            NSString* entry = [NSString stringWithUTF8String:path.c_str()];
            NSUserDefaults* ud = [NSUserDefaults standardUserDefaults];
            NSArray<NSString*>* existing = [ud stringArrayForKey:k_recentFilesKey];
            NSMutableArray<NSString*>* list =
                existing ? [existing mutableCopy] : [NSMutableArray array];
            [list removeObject:entry];           // remove any existing occurrence
            [list insertObject:entry atIndex:0]; // promote to top of MRU list
            while (list.count > k_maxRecentFiles)
                [list removeLastObject];
            [ud setObject:[list copy] forKey:k_recentFilesKey];
        }
    }

    void removeRecentFile(const std::filesystem::path& path) override {
        @autoreleasepool {
            NSString* entry = [NSString stringWithUTF8String:path.c_str()];
            NSUserDefaults* ud = [NSUserDefaults standardUserDefaults];
            NSArray<NSString*>* existing = [ud stringArrayForKey:k_recentFilesKey];
            if (!existing) return;
            NSMutableArray<NSString*>* list = [existing mutableCopy];
            [list removeObject:entry];
            [ud setObject:[list copy] forKey:k_recentFilesKey];
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<IFileSystem> IFileSystem::create() {
    return std::make_unique<MacFileSystem>();
}

} // namespace dexilate::platform
