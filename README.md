# WallpaperAnim

[![Build](https://github.com/hamer1818/wallpaper-anim/actions/workflows/build.yml/badge.svg)](https://github.com/hamer1818/wallpaper-anim/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/hamer1818/wallpaper-anim)](https://github.com/hamer1818/wallpaper-anim/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A lightweight animated wallpaper engine for Windows 10/11 and Linux (Wayland). Play MP4 videos, GIFs, and raw shaders behind your desktop icons — with built‑in YouTube downloading, a thumbnail library, and smart resource saving.

## Features

- **Video, GIF & shader wallpapers** — `.mp4`, `.avi`, `.mkv`, `.gif`, and Shadertoy‑style `.hlsl` pixel shaders, rendered on the GPU behind your desktop icons.
- **YouTube integration** — paste a link and it downloads the best **H.264** stream that fits your screen (up to 1080p, 60 fps when the source offers it), so it plays on every Windows version without extra codecs.
- **Thumbnail library** — every wallpaper you add is remembered with a generated thumbnail; one click to switch, one click to remove.
- **Playlists & auto‑rotation** — group wallpapers into named playlists and cycle through the active one on a timer (5–60 min, optional shuffle); leave no playlist selected to rotate the whole library.
- **Fit modes** — per‑monitor **Fill** (crop), **Fit** (letterbox), **Stretch**, or **Center**, so any aspect ratio looks right without distortion.
- **Low resource use** — zero‑copy GPU video decode (NV12 kept on the GPU, YUV→RGB in a shader) and present‑on‑new‑frame pacing keep CPU near idle; frames stay VSync‑paced (no judder, even on 144 Hz+). Choose **Maximum (monitor Hz)**, 60, or 30 FPS.
- **Multi‑monitor** — the wallpaper spans every screen, drawn full‑size per monitor.
- **Smart auto‑pause** — stops rendering when the wallpaper can't be seen anyway: a fullscreen app/game, a maximized window covering the desktop (single‑monitor), or the device on battery.
- **Auto‑update** — checks GitHub Releases, verifies the download (HTTPS + SHA‑256), and updates in place.
- **Modern UI** — a clean WinUI 3 settings window, with Turkish and English localization.
- **System tray** — play/pause, open settings, and quit from the tray.
- **Linux (Wayland)** — a native port with the same features, rendered with libmpv; see [Linux](#linux) below.

## Installation

1. Go to the [**latest release**](https://github.com/hamer1818/wallpaper-anim/releases/latest).
2. Download either:
   - **`WallpaperAnimSetup.exe`** — installs to `%LocalAppData%\WallpaperAnim` and creates shortcuts, or
   - **`WallpaperAnim-Portable.zip`** — extract anywhere and run `WallpaperAnimWinUI.exe`.
3. `yt-dlp.exe` and `ffmpeg.exe` are bundled in the release — keep them next to the executable to use the YouTube feature.

Requires Windows 10 (1809+) or Windows 11, x64.

## Linux

A native Wayland port lives in [`linux/`](linux/) — same library, playlists, fit modes and
`config.json` schema, rendered with **libmpv** behind a **Qt 6** settings UI.

```sh
sudo pacman -S --needed qt6-base qt6-multimedia qt6-declarative mpv layer-shell-qt \
                        ffmpeg cmake ninja pkgconf
cd linux && ./build.sh user-install
```

Two backends cover the two ways a Wayland desktop can put pixels behind the icons:

- **KDE Plasma** — a small QML wallpaper plugin runs inside plasmashell, so icons and
  widgets stay on top. Applying a wallpaper switches the desktop over automatically;
  *Settings → Display Method* can hand it back to Plasma's own wallpaper. (An outside
  layer-shell surface cannot work on KDE: on the background layer KWin stacks it beneath
  plasmashell's opaque desktop window, and on the bottom layer it covers the icons.)
- **wlroots compositors** (Hyprland, Sway, …) — a `zwlr_layer_shell_v1` background
  surface per output, which is also the backend that runs GLSL shaders.

Shaders on Linux are GLSL (`.glsl`, `.frag`) rather than HLSL; see
[`assets/sample.glsl`](assets/sample.glsl). Full details, including troubleshooting, are in
[`linux/README.md`](linux/README.md).

## Building from source

The project is native **C++/WinRT (WinUI 3)** for the UI plus **Win32 + DirectX 11 + Media Foundation** for rendering. It builds from a single `.vcxproj` (no solution file).

You'll need **Visual Studio 2022** with the *Desktop development with C++* workload and the **Windows App SDK** / C++/WinRT build tools (restored via NuGet). Then, from a Developer Command Prompt at the repo root:

```sh
nuget restore packages.config -PackagesDirectory packages
build.bat            # builds Release|x64 -> x64\Release\WallpaperAnimWinUI.exe
```

`build.bat` locates `vcvarsall.bat`, closes any running instance, and builds. For the YouTube feature, place [`yt-dlp.exe`](https://github.com/yt-dlp/yt-dlp/releases/latest) and [`ffmpeg.exe`](https://github.com/BtbN/FFmpeg-Builds/releases) next to the built executable (CI downloads these automatically for release builds). Both are git‑ignored and never committed.

## Releases (CI)

Releases are fully automated by [GitHub Actions](.github/workflows/build.yml): the version is the single source of truth in [`src/version.h`](src/version.h). On every push to `master`, CI builds Release/x64, produces the Setup executable and the Portable zip, and — if that version has no release yet — publishes a new GitHub Release with both assets. **To ship a new version, bump `src/version.h` and push.**

## Architecture

Two layers run in one process:

- A **WinUI 3 settings window** (`App`, `MainWindow`) — the visible UI.
- A hidden **Win32 + DirectX 11 wallpaper window**, parented into `WorkerW`/`Progman` so it renders behind the desktop icons on a background thread.

They communicate via Win32 messages posted to the wallpaper window. Media playback goes through an `IMediaPlayer` interface with `VideoPlayer` (Media Foundation), `GifPlayer`, and `ShaderPlayer` implementations, selected by file extension. See [`CLAUDE.md`](CLAUDE.md) for a deeper tour.

```text
src/                         # Windows build
├── config.*                 # JSON settings + wallpaper history (%LocalAppData%)
├── localization.h           # TR/EN strings
├── tray.*                   # system tray icon & menu
├── desktop/                 # WorkerW/Progman desktop integration
├── render/                  # DX11 renderer + video/gif/shader players
├── system/                  # fullscreen & battery detection
└── utils/                   # YouTube downloader, updater, thumbnails, textures

linux/                       # Linux (Wayland) build
├── src/                     # Qt 6 UI + libmpv renderer + layer-shell surface
└── data/plasma/wallpapers/  # QML wallpaper plugin for the KDE backend
```

## Tech stack

**Windows:** DirectX 11 · Media Foundation · Win32 · C++/WinRT (WinUI 3, Windows App SDK)

**Linux:** Qt 6 · [libmpv](https://mpv.io/) · wlr-layer-shell ([LayerShellQt](https://invent.kde.org/plasma/layer-shell-qt)) · KDE Plasma wallpaper plugin (QML)

**Both:** [nlohmann/json](https://github.com/nlohmann/json) · [stb](https://github.com/nothings/stb) · [yt-dlp](https://github.com/yt-dlp/yt-dlp) · [FFmpeg](https://ffmpeg.org/)

## License

[MIT](LICENSE)
