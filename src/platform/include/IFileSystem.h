// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  dexilate/platform/IFileSystem.h
//
//  Platform-agnostic file system abstraction (Layer 2 PAL).
//
//  Wraps OS-native file dialogs, recent-files tracking, atomic saves, and
//  temporary directory access. Engine and codec code never calls OS file
//  APIs directly.
//
//  Thread safety: All methods must be called from the main thread unless
//  explicitly noted. File I/O operations (readFile, writeFileAtomic) are
//  synchronous but may be called from a worker thread.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dexilate::platform {

// ── File filter (for open/save dialogs) ──────────────────────────────────────
struct FileFilter {
    std::string name;                  ///< e.g. "Dexilate Documents"
    std::vector<std::string> patterns; ///< e.g. {"*.canvas", "*.psd"}
};

// ── Open dialog options ───────────────────────────────────────────────────────
struct OpenDialogOptions {
    std::string              title        = "Open File";
    std::vector<FileFilter>  filters;           ///< Empty = all files
    bool                     allowMultiple = false;
    std::filesystem::path    initialDirectory;  ///< Empty = OS default
};

// ── Save dialog options ───────────────────────────────────────────────────────
struct SaveDialogOptions {
    std::string              title           = "Save File";
    std::vector<FileFilter>  filters;
    std::string              defaultFilename;
    std::filesystem::path    initialDirectory;
};

// ─────────────────────────────────────────────────────────────────────────────
//  IFileSystem
// ─────────────────────────────────────────────────────────────────────────────
class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    // ── Native file dialogs ───────────────────────────────────────────────────
    /// Show the OS open-file dialog. Returns selected path(s), or empty on cancel.
    /// Main thread only (runs a modal OS dialog).
    [[nodiscard]] virtual std::vector<std::filesystem::path>
        showOpenDialog(const OpenDialogOptions& options) = 0;

    /// Show the OS save-file dialog. Returns the chosen path, or nullopt on cancel.
    /// Main thread only.
    [[nodiscard]] virtual std::optional<std::filesystem::path>
        showSaveDialog(const SaveDialogOptions& options) = 0;

    // ── File I/O ──────────────────────────────────────────────────────────────
    /// Read the entire file into memory. Worker-thread safe.
    [[nodiscard]] virtual std::vector<uint8_t>
        readFile(const std::filesystem::path& path) = 0;

    /// Write data atomically: write to a temp file, then rename over the target.
    /// Prevents data loss if the process is killed mid-write. Worker-thread safe.
    virtual void writeFileAtomic(const std::filesystem::path& path,
                                 const uint8_t* data,
                                 size_t         size) = 0;

    // ── Directories ───────────────────────────────────────────────────────────
    /// Returns the OS-appropriate temp directory (e.g. /tmp, %TEMP%).
    [[nodiscard]] virtual std::filesystem::path tempDirectory() const = 0;

    /// Returns the user's documents directory (macOS ~/Documents, Windows My Documents).
    [[nodiscard]] virtual std::filesystem::path documentsDirectory() const = 0;

    /// Returns the app's writable data directory for settings, caches, etc.
    ///   macOS   → ~/Library/Application Support/Dexilate/
    ///   Windows → %APPDATA%/Dexilate/
    ///   Linux   → ~/.local/share/Dexilate/ (XDG_DATA_HOME)
    [[nodiscard]] virtual std::filesystem::path appDataDirectory() const = 0;

    // ── Recent files ──────────────────────────────────────────────────────────
    /// Returns paths in most-recently-used order.
    [[nodiscard]] virtual std::vector<std::filesystem::path> recentFiles() const = 0;

    /// Add a path to the top of the recent files list (duplicates moved to top).
    virtual void addRecentFile(const std::filesystem::path& path) = 0;

    /// Remove a path from the recent files list (e.g. file no longer exists).
    virtual void removeRecentFile(const std::filesystem::path& path) = 0;

    // ── Factory ───────────────────────────────────────────────────────────────
    [[nodiscard]] static std::unique_ptr<IFileSystem> create();
};

} // namespace dexilate::platform
