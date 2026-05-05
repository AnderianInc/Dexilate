// SPDX-License-Identifier: MIT
// Dexilate — Professional Creative Suite
// Copyright (c) 2026 Dexilate Software

// ─────────────────────────────────────────────────────────────────────────────
//  src/platform/src/windows/Win32FileSystem.cpp
//  IFileSystem implementation for Windows
//
//  Dialogs:     IFileOpenDialog / IFileSaveDialog (Shell COM, Vista+).
//               CoInitializeEx called on demand — harmless if already done.
//  File I/O:    std::fstream — worker-thread safe; no COM involved.
//  Atomic save: write to ".dexilate_tmp" sibling, then MoveFileExW with
//               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH.
//               On NTFS, same-volume move is atomic for the file entry.
//  Directories: SHGetKnownFolderPath (FOLDERID_*) + GetTempPathW.
//  Recent files: HKCU\Software\Dexilate\RecentFiles (REG_MULTI_SZ, max 20).
// ─────────────────────────────────────────────────────────────────────────────

#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>       // SHGetKnownFolderPath, IFileOpenDialog
#include <shobjidl.h>     // IFileOpenDialog, IFileSaveDialog, CLSID_*
#include <knownfolders.h> // FOLDERID_Documents, FOLDERID_RoamingAppData

#include "dexilate/platform/IFileSystem.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <system_error>

// Microsoft::WRL::ComPtr — RAII for COM interface pointers
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

namespace dexilate::platform {

// ─────────────────────────────────────────────────────────────────────────────
namespace {

constexpr size_t      k_maxRecent = 20;
constexpr const wchar_t* k_regKey   = L"Software\\Dexilate";
constexpr const wchar_t* k_regValue = L"RecentFiles";

std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
        static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
        static_cast<int>(s.size()), w.data(), n);
    return w;
}

// Holds COMDLG_FILTERSPEC data with stable string storage.
struct FilterSpec {
    std::vector<std::wstring>      names;
    std::vector<std::wstring>      specs;
    std::vector<COMDLG_FILTERSPEC> raw;

    explicit FilterSpec(const std::vector<FileFilter>& filters) {
        for (const auto& f : filters) {
            names.push_back(toWide(f.name));
            std::wstring combined;
            for (size_t i = 0; i < f.patterns.size(); ++i) {
                if (i > 0) combined += L';';
                combined += toWide(f.patterns[i]);
            }
            specs.push_back(std::move(combined));
        }
        raw.resize(names.size());
        for (size_t i = 0; i < names.size(); ++i) {
            raw[i].pszName = names[i].c_str();
            raw[i].pszSpec = specs[i].c_str();
        }
    }
};

std::filesystem::path knownFolder(const KNOWNFOLDERID& id) {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &path))) {
        std::filesystem::path result(path);
        CoTaskMemFree(path);
        return result;
    }
    return {};
}

// Ensure COM is initialised on the calling thread.
// Returns true if we initialised it (and thus own a CoUninitialize call).
bool coInit() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    return SUCCEEDED(hr);  // S_OK (first init) or S_FALSE (already init) both succeed
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
class Win32FileSystem final : public IFileSystem {
public:
    // ── Dialogs ───────────────────────────────────────────────────────────────
    std::vector<std::filesystem::path>
    showOpenDialog(const OpenDialogOptions& opts) override {
        coInit();
        ComPtr<IFileOpenDialog> dlg;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
            return {};

        dlg->SetTitle(toWide(opts.title).c_str());

        FILEOPENDIALOGOPTIONS flags = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
        if (opts.allowMultiple) flags |= FOS_ALLOWMULTISELECT;
        dlg->SetOptions(flags);

        FilterSpec fs(opts.filters);
        if (!fs.raw.empty())
            dlg->SetFileTypes(static_cast<UINT>(fs.raw.size()), fs.raw.data());

        if (!opts.initialDirectory.empty()) {
            ComPtr<IShellItem> dir;
            if (SUCCEEDED(SHCreateItemFromParsingName(
                    opts.initialDirectory.wstring().c_str(),
                    nullptr, IID_PPV_ARGS(&dir))))
                dlg->SetFolder(dir.Get());
        }

        std::vector<std::filesystem::path> result;
        if (FAILED(dlg->Show(nullptr))) return result;  // user cancelled

        ComPtr<IShellItemArray> items;
        if (FAILED(dlg->GetResults(&items))) return result;
        DWORD count = 0;
        items->GetCount(&count);
        for (DWORD i = 0; i < count; ++i) {
            ComPtr<IShellItem> item;
            if (FAILED(items->GetItemAt(i, &item))) continue;
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result.emplace_back(path);
                CoTaskMemFree(path);
            }
        }
        return result;
    }

    std::optional<std::filesystem::path>
    showSaveDialog(const SaveDialogOptions& opts) override {
        coInit();
        ComPtr<IFileSaveDialog> dlg;
        if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr,
                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
            return std::nullopt;

        dlg->SetTitle(toWide(opts.title).c_str());

        if (!opts.defaultFilename.empty())
            dlg->SetFileName(toWide(opts.defaultFilename).c_str());

        FilterSpec fs(opts.filters);
        if (!fs.raw.empty()) {
            dlg->SetFileTypes(static_cast<UINT>(fs.raw.size()), fs.raw.data());
            dlg->SetFileTypeIndex(1);  // select first filter by default
        }

        if (!opts.initialDirectory.empty()) {
            ComPtr<IShellItem> dir;
            if (SUCCEEDED(SHCreateItemFromParsingName(
                    opts.initialDirectory.wstring().c_str(),
                    nullptr, IID_PPV_ARGS(&dir))))
                dlg->SetFolder(dir.Get());
        }

        if (FAILED(dlg->Show(nullptr))) return std::nullopt;

        ComPtr<IShellItem> item;
        if (FAILED(dlg->GetResult(&item))) return std::nullopt;
        PWSTR path = nullptr;
        if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
            return std::nullopt;
        std::filesystem::path result(path);
        CoTaskMemFree(path);
        return result;
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
        tmp += L".dexilate_tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f)
                throw std::system_error(errno, std::generic_category(), tmp.string());
            f.write(reinterpret_cast<const char*>(data),
                    static_cast<std::streamsize>(size));
            if (!f)
                throw std::system_error(errno, std::generic_category(), tmp.string());
        }
        // MOVEFILE_WRITE_THROUGH flushes OS buffers before returning.
        // MOVEFILE_REPLACE_EXISTING atomically replaces the destination.
        if (!MoveFileExW(tmp.wstring().c_str(), path.wstring().c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            DeleteFileW(tmp.wstring().c_str());
            throw std::system_error(static_cast<int>(GetLastError()),
                std::system_category(), path.string());
        }
    }

    // ── Directories ───────────────────────────────────────────────────────────
    std::filesystem::path tempDirectory() const override {
        wchar_t buf[MAX_PATH + 1] = {};
        GetTempPathW(MAX_PATH, buf);
        return {buf};
    }

    std::filesystem::path documentsDirectory() const override {
        auto p = knownFolder(FOLDERID_Documents);
        return p.empty() ? std::filesystem::path{L"C:\\Users\\Public\\Documents"} : p;
    }

    std::filesystem::path appDataDirectory() const override {
        auto base = knownFolder(FOLDERID_RoamingAppData);
        if (base.empty()) base = L"C:\\Users\\Default\\AppData\\Roaming";
        auto appDir = base / "Dexilate";
        std::filesystem::create_directories(appDir);
        return appDir;
    }

    // ── Recent files — HKCU\Software\Dexilate\RecentFiles (REG_MULTI_SZ) ────────
    std::vector<std::filesystem::path> recentFiles() const override {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, k_regKey, 0, KEY_READ, &hKey)
                != ERROR_SUCCESS)
            return {};

        DWORD type = REG_MULTI_SZ, sz = 0;
        RegQueryValueExW(hKey, k_regValue, nullptr, &type, nullptr, &sz);
        std::vector<std::filesystem::path> out;
        if (sz > 0) {
            std::vector<wchar_t> buf(sz / sizeof(wchar_t) + 1, L'\0');
            if (RegQueryValueExW(hKey, k_regValue, nullptr, &type,
                    reinterpret_cast<BYTE*>(buf.data()), &sz) == ERROR_SUCCESS) {
                for (const wchar_t* p = buf.data(); *p; p += wcslen(p) + 1)
                    out.emplace_back(p);
            }
        }
        RegCloseKey(hKey);
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
        // Build REG_MULTI_SZ: null-terminated wstrings, double-null at end
        std::vector<wchar_t> buf;
        for (const auto& p : list) {
            auto ws = p.wstring();
            buf.insert(buf.end(), ws.begin(), ws.end());
            buf.push_back(L'\0');
        }
        buf.push_back(L'\0');  // final double-null terminator

        HKEY hKey = nullptr;
        RegCreateKeyExW(HKEY_CURRENT_USER, k_regKey, 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
        if (hKey) {
            RegSetValueExW(hKey, k_regValue, 0, REG_MULTI_SZ,
                reinterpret_cast<const BYTE*>(buf.data()),
                static_cast<DWORD>(buf.size() * sizeof(wchar_t)));
            RegCloseKey(hKey);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
std::unique_ptr<IFileSystem> IFileSystem::create() {
    return std::make_unique<Win32FileSystem>();
}

} // namespace dexilate::platform
