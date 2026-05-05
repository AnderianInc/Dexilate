# Dexilate

Professional creative suite — raster + vector image editor.

A single native application replacing Adobe Photoshop + Illustrator. No subscriptions. Perpetual license. Runs natively on macOS, Windows, and Linux.

**Status**: Pre-development (Phase 0 — toolchain & foundation)

---

## Building

See [CONTRIBUTING.md](CONTRIBUTING.md) for full setup instructions.

**Quick start (macOS):**
```sh
# Install Qt6 and set Qt6_DIR
export Qt6_DIR=/opt/homebrew/opt/qt/lib/cmake/Qt6

cmake --preset macos-arm64
cmake --build build/macos-arm64 --parallel
```

**Available presets:** `macos-arm64`, `macos-x64`, `macos-universal`, `windows-x64`, `linux-x64`, `linux-arm64`

---

## Architecture

Five-layer dependency model — higher layers depend on lower; lower layers have no knowledge of higher:

| Layer | Name | Responsibility |
|-------|------|----------------|
| L5 | UI Layer | Qt6 shell: toolbars, panels, canvas widget |
| L4 | Engine Layer | Raster/vector engine, document model |
| L3 | Core Library | Codecs, color management, text shaping |
| L2 | Platform Abstraction | Window, input, filesystem, clipboard |
| L1 | GPU Layer | wgpu-native → Metal / D3D12 / Vulkan |

---

## License

MIT — see [LICENSE](LICENSE).
