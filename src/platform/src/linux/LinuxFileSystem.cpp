// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/platform/src/linux/LinuxFileSystem.cpp
//  IFileSystem implementation for Linux
//
//  Dialogs:     zenity(1) subprocess — available on most desktop Linux
//               distros (GNOME/KDE). Falls back gracefully when not found.
//               Phase 1 will replace with libportal (xdg-desktop-portal).
//  File I/O:    std::fstream — worker-thread safe.
//  Atomic save: write to ".dexilate_tmp" sibling, then rename(2).
//               POSIX rename is atomic on ext4/btrfs/xfs/tmpfs for same-fs.
//  Directories: XDG Base Directory Specification:
//                 temp     → $TMPDIR or /tmp
//                 documents→ $XDG_DOCUMENTS_DIR or ~/Documents
//                 appdata  → $XDG_DATA_HOME/Dexilate or ~/.local/share/Dexilate
//  Recent files: $XDG_DATA_HOME/Dexilate/recent_files.txt (one path per line,
//               MRU order, max 20 entries). Plain text is portable and
//               inspectable without any additional library.
// ─────────────────────────────────────────────────────────────────────────────

#include "dexilate/platform/IFileSystem.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace dexilate::platform {

// ─────────────────────────────────────────────────────────────────────────────
namespace {

constexpr size_t k_maxRecent = 20;

// XDG directory helpers
std::filesystem::path xdgDataHome() {
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg)
        return xdg;
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : "/tmp") / ".local" / "share";
}

std::filesystem::path xdgDocuments() {
    const char* docs = std::getenv("XDG_DOCUMENTS_DIR");
    if (docs && *docs)
        return docs;
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : "/tmp") / "Documents";
}

std::filesystem::path appDataDir() {
    auto dir = xdgDataHome() / "Dexilate";
    std::filesystem::create_directories(dir);
    return dir;
}

std::filesystem::path recentFilesPath() {
    return appDataDir() / "recent_files.txt";
}

// Run `zenity` with the given arguments and return its stdout output.
// Returns empty string if zenity is not found or the dialog is cancelled.
std::string runZenity(const std::vector<std::string>& args) {
    std::string cmd = "zenity";
    for (const auto& a : args) {
        cmd += ' ';
        // Simple shell-quote: wrap in single quotes, escape embedded '
        cmd += '\'';
        for (char c : a) {
            if (c == '\'') cmd += "'\\''";
            else           cmd += c;
        }
        cmd += '\'';
    }
    cmd += " 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};

    std::string result;
    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe))
        result += buf;
    pclose(pipe);

    // zenity appends a newline; strip it
    if (!result.empty() && result.back() == '\n')
        result.pop_back();
    return result;
}

// Build --file-filter args from FileFilter list.
// zenity format: "Name | *.ext1 *.ext2"
std::vector<std::string> buildZenityFilters(const std::vector<FileFilter>& filters) {
    std::vector<std::string> args;
    for (const auto& f : filters) {
        std::string spec = f.name + " |";
        for (const auto& p : f.patterns)
            spec += ' ' + p;
        args.push_back("--file-filter=" + spec);
    }
    // Always offer an "All files" fallback when filters are specified
    if (!filters.empty())
        args.push_back("--file-filter=All files | *");
    return args;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
class LinuxFileSystem final : public IFileSystem {
public:
    // ── Dialogs ───────────────────────────────────────────────────────────────
    std::vector<std::filesystem::path>
    showOpenDialog(const OpenDialogOptions& opts) override {
        std::vector<std::string> args = {
            "--file-selection",
            "--title=" + opts.title
        };
        if (opts.allowMultiple) args.push_back("--multiple");
        if (!opts.initialDirectory.empty())
            args.push_back("--filename=" + opts.initialDirectory.string() + "/");

        auto filters = buildZenityFilters(opts.filters);
        args.insert(args.end(), filters.begin(), filters.end());

        std::string output = runZenity(args);
        if (output.empty()) return {};

        std::vector<std::filesystem::path> result;
        if (opts.allowMultiple) {
            // zenity --multiple outputs paths separated by '|'
            std::istringstream ss(output);
            std::string tok;
            while (std::getline(ss, tok, '|'))
                if (!tok.empty()) result.emplace_back(tok);
        } else {
            result.emplace_back(output);
        }
        return result;
    }

    std::optional<std::filesystem::path>
    showSaveDialog(const SaveDialogOptions& opts) override {
        std::vector<std::string> args = {
            "--file-selection",
            "--save",
            "--confirm-overwrite",
            "--title=" + opts.title
        };
        if (!opts.defaultFilename.empty()) {
            auto init = opts.initialDirectory.empty()
                ? std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "/tmp")
                : opts.initialDirectory;
            args.push_back("--filename=" + (init / opts.defaultFilename).string());
        } else if (!opts.initialDirectory.empty()) {
            args.push_back("--filename=" + opts.initialDirectory.string() + "/");
        }

        auto filters = buildZenityFilters(opts.filters);
        args.insert(args.end(), filters.begin(), filters.end());

        std::string output = runZenity(args);
        if (output.empty()) return std::nullopt;
        return std::filesystem::path{output};
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
                         const uint8_t* data, size_t size) override {
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
        // POSIX rename(2) is atomic for same-filesystem moves
        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            std::filesystem::remove(tmp);
            throw std::system_error(ec, path.string());
        }
    }

    // ── Directories ───────────────────────────────────────────────────────────
    std::filesystem::path tempDirectory() const override {
        const char* tmpdir = std::getenv("TMPDIR");
        return tmpdir ? std::filesystem::path(tmpdir) : std::filesystem::path("/tmp");
    }

    std::filesystem::path documentsDirectory() const override {
        return xdgDocuments();
    }

    std::filesystem::path appDataDirectory() const override {
        return appDataDir();
    }

    // ── Recent files — plain text, one path per line, MRU order ───────────────
    std::vector<std::filesystem::path> recentFiles() const override {
        std::ifstream f(recentFilesPath());
        std::vector<std::filesystem::path> out;
        std::string line;
        while (std::getline(f, line))
            if (!line.empty()) out.emplace_back(line);
        return out;
    }

    void addRecentFile(const std::filesystem::path& path) override {
        auto list = recentFiles();
        auto it = std::remove_if(list.begin(), list.end(),
            [&path](const auto& p) { return p == path; });
        list.erase(it, list.end());
        list.insert(list.begin(), path);
        if (list.size() > k_maxRecent) list.resize(k_maxRecent);
        writeRecentFiles(list);
    }

    void removeRecentFile(const std::filesystem::path& path) override {
        auto list = recentFiles();
        auto it = std::remove_if(list.begin(), list.end(),
            [&path](const auto& p) { return p == path; });
        list.erase(it, list.end());
        writeRecentFiles(list);
    }

private:
    void writeRecentFiles(const std::vector<std::filesystem::path>& list) {
        auto p = recentFilesPath();
        std::ofstream f(p, std::ios::trunc);
        for (const auto& path : list)
            f << path.string() << '\n';
    }
};

// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<IFileSystem> IFileSystem::create() {
    return std::make_unique<LinuxFileSystem>();
}

} // namespace dexilate::platform
