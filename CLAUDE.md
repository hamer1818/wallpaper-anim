# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

WallpaperAnim is a lightweight animated wallpaper engine for Windows 10/11. It renders MP4 videos, GIFs, and raw HLSL shaders behind desktop icons, with native YouTube downloading, a thumbnail library, and auto-pause to save resources. The goal is minimal resource usage (see targets in [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md)).

Note: the README and IMPLEMENTATION_PLAN describe an earlier pure-Win32 + ImGui design. The project has since **migrated to WinUI 3 (C++/WinRT)** for the settings UI while keeping the Win32/DirectX rendering core. The old ImGui codebase lives in [old_imgui_project/](old_imgui_project/) and is not built. IMPLEMENTATION_PLAN.md is in Turkish and is gitignored/historical.

## Build & Release

There is **no `.sln`** — the project is a single vcxproj built directly with MSBuild.

```sh
# Full build (finds vcvarsall.bat, then builds Release x64). Output: x64\Release\WallpaperAnimWinUI.exe
build.bat

# Manual equivalent (from a Developer Command Prompt / after vcvarsall amd64):
msbuild WallpaperAnimWinUI.vcxproj /p:Configuration=Release /p:Platform=x64
# Configurations: Debug|x64 and Release|x64 only.

# NuGet packages (CppWinRT, WindowsAppSDK, SDK BuildTools) are restored into packages\.
# vcxproj imports them by hardcoded version path; if restore is needed:
nuget.exe restore packages.config -PackagesDirectory packages
```

Release packaging (run **after** a Release build so `x64\Release` is current):

```powershell
# Builds WallpaperAnimSetup.exe: a .NET WinForms installer (setup.cs) that embeds
# the whole x64\Release payload as an "app.zip" resource and extracts to %LocalAppData%\WallpaperAnim.
.\build_setup.ps1

# Creates WallpaperAnim-Portable.zip and publishes a GitHub release via `gh release create`.
# Edit the hardcoded version tag/notes in this script before running.
.\release.ps1
```

There are no automated tests. Verification is manual (run the app, confirm wallpaper renders behind icons, test auto-pause, YouTube download, updater).

## Versioning

Version is single-sourced in [src/version.h](src/version.h) (`APP_VERSION_*` macros). The UI reads `APP_VERSION_STRING_W`. When bumping the version, update version.h **and** the tag/notes in `release.ps1`.

## Architecture

Two cooperating layers run in one process:

1. **WinUI 3 settings UI** (`App`, `MainWindow`) — the visible XAML window.
2. **Win32 + DirectX 11 wallpaper renderer** — a hidden `WS_POPUP` window parented into `WorkerW` so it draws behind desktop icons.

### Entry & lifecycle
- [main.cpp](main.cpp) — `wWinMain` boots the WinRT apartment and calls `Application::Start` → constructs `App`.
- [App.xaml.cpp](App.xaml.cpp) `App::OnLaunched` is the real orchestrator. It:
  1. Loads config, creates the wallpaper `HWND` (window class `WallpaperAnimClassWinUI`, proc `WallpaperWindowProc`).
  2. Initializes `DX11Renderer`, then `DesktopIntegration::SetupWallpaperWindow` (WorkerW parenting).
  3. Initializes the tray icon and loads initial media.
  4. Spawns the **render thread** (`App::RenderLoop`).
  5. Creates and activates the `MainWindow` (settings UI).
- `App` is a singleton exposed via `App::GetInstance()` / `GetWallpaperHwnd()` so the UI can reach the wallpaper window.

### Rendering
- [src/render/dx11_renderer.*](src/render/) owns the D3D11 device/context/swap chain and holds the active media player as a `std::unique_ptr<IMediaPlayer>`. `SetMediaPlayer` swaps players under a mutex (`m_mediaMutex`) that also guards `RenderFrame`, so the render thread never touches a freed player.
- [src/render/media_player.h](src/render/media_player.h) defines the `IMediaPlayer` interface (`Initialize/LoadMedia/Play/Pause/Stop/UpdateFrame/Render/Cleanup`). Implementations: `VideoPlayer` (Media Foundation), `GifPlayer`, `ShaderPlayer` (HLSL). `App::LoadMedia` picks the implementation by file extension (`.gif`→GifPlayer, `.hlsl`→ShaderPlayer, else VideoPlayer). Add new media types here + a new `IMediaPlayer`.
- **Multi-monitor**: the wallpaper window and swap chain span the whole *virtual desktop* (`SM_*VIRTUALSCREEN`). `DX11Renderer` computes one `D3D11_VIEWPORT` per monitor (`UpdateMonitorLayout`, refreshed on `Resize`); `RenderFrame` calls `UpdateFrame()` once then draws the media into each viewport so every monitor shows a full-size (non-stretched) copy. Caveat: viewports are placed relative to the virtual-desktop origin, so monitors positioned left/above the primary (negative-origin layouts) may be offset — right/below layouts are exact.
- `App::RenderLoop` runs on its own thread: once per second it checks `SystemMonitor` (battery / fullscreen app) for auto-pause and refreshes the `maxFPS` budget. It renders + presents otherwise, **frame-limited to `config.maxFPS`** (VSync alone would run at monitor refresh). Manual pause (`m_isPaused`) comes from the tray. `WM_DISPLAYCHANGE` resizes the window + swap chain to the new virtual-desktop size. NOTE: DXGI `DXGI_STATUS_OCCLUDED`-based pausing was removed — a WorkerW-parented wallpaper window can report itself permanently occluded (seen on Windows 10), which stopped the wallpaper from ever rendering. `DX11Renderer::Present`/`TestOcclusion` still return the HRESULT but the loop no longer acts on it.

### UI ↔ renderer communication
The two layers communicate via **Win32 messages posted to the wallpaper HWND**, not shared objects:
- `WM_APP_CONFIG_CHANGED` (defined in [App.xaml.h](App.xaml.h)) — UI posts this after changing `lastVideoPath`; `HandleWallpaperMessage` reloads media.
- Tray messages (`SystemTray::WM_TRAYICON`, `ID_TRAY_*`) drive play/pause/settings/exit.
When the UI changes the active wallpaper, it saves config then `PostMessage(App::GetInstance()->GetWallpaperHwnd(), WM_APP_CONFIG_CHANGED, 0, 0)`.

### Config & state
- [src/config.*](src/config.cpp) — `ConfigManager` singleton, JSON at `%LocalAppData%\WallpaperAnim\config.json`. `AppConfig` holds settings + a `history` vector of `WallpaperHistoryItem` (path/name/thumbPath/type) that backs the Library grid. The JSON carries a `configVersion` (`kCurrentConfigVersion` in config.h) for future migrations; `history` is capped to `kMaxHistoryItems` (enforced centrally in `Save`, and clamped on `Load`).
- Deleting a library item (`BtnDeleteWallpaper_Click`) also removes its thumbnail file, and the media file itself only for app-downloaded YouTube items (`type == 3`) — never the user's own local files.
- Startup-on-boot is a registry value `WallpaperAnim` under `HKCU\...\CurrentVersion\Run` (managed in `MainWindow::TglStartup_Toggled`).

### Supporting subsystems (src/)
- `desktop/desktop_integration.*` — WorkerW/Progman hooking to render behind icons, and restore on exit.
- `system/system_monitor.*` — `IsOnBattery`, `IsFullscreenAppActive` for auto-pause.
- `utils/youtube_downloader.*` — wraps bundled `yt-dlp.exe`. `DownloadAsync` deliberately selects an **H.264 (avc1) stream capped at the screen height** (≤1080p), not the highest resolution: Media Foundation can't decode YouTube's high-res VP9/AV1 on stock Windows 10, so downloading 4K would produce a wallpaper that silently fails to play there. yt-dlp.exe must ship next to the exe (copied by vcxproj, gitignored in repo).
- `utils/update_checker.*` — GitHub-release auto-update. Downloads the ZIP asset over HTTPS only, verifies it against the asset's `digest` (SHA-256 via BCrypt, `UpdateInfo::sha256`) when present, refuses to apply a package that lacks `WallpaperAnimWinUI.exe`, then extracts and launches a batch script that swaps files and restarts.
- `utils/thumbnail_generator.*`, `texture_loader.*`, `preview_player.*` — library thumbnails and previews.
- `tray.*` — `Shell_NotifyIcon` tray icon and menu.

### Localization
UI strings come from [src/localization.h](src/localization.h) via `Localization::Get()`, switched by `config.language` (`tr`/`en`, empty = auto). `MainWindow::LoadLocalization()` reapplies all strings on language change. The `Strings` struct uses **positional aggregate initialization** — when adding a key, append it to the struct *and* to both the `tr` and `en` initializer lists in the same position (append at the end to avoid miscounting). Download/update progress status text is localized via these keys.

## Linux port (`linux/`)

The Linux build is a **separate program in the same repository** — no Windows source file
is compiled into it. It is Qt 6 Widgets for the UI and **libmpv** for playback, and it
reads/writes the *same* `config.json` schema (UTF-8 `std::string` instead of
`std::wstring`, plus `linux*`-prefixed keys for Linux-only settings).

```sh
cd linux
./build.sh                 # cmake+ninja -> linux/build/wallpaperanim
./build.sh user-install    # install into ~/.local
./package.sh               # -> linux/dist/wallpaperanim-<version>-x86_64.pkg.tar.zst
cd packaging && makepkg -si  # Arch/CachyOS package, installed in place
```

`package.sh` is the Linux counterpart of `build_setup.ps1`: it wraps `makepkg` (PKGDEST
into `linux/dist`, scratch trees out of the checkout) so the output is a single file the
recipient installs with `sudo pacman -U`. `PKGBUILD` reads `pkgver` out of the repo root's
`src/version.h`, so bumping the version there covers the package too.

Deps: `qt6-base qt6-multimedia qt6-declarative mpv layer-shell-qt ffmpeg` (+ optional
`yt-dlp`). `layer-shell-qt` is optional at build time (guarded by `WPA_HAVE_LAYER_SHELL`).

### The two backends, and why

Wayland has no `WorkerW` equivalent, and the right mechanism differs per compositor.
This was settled empirically on Plasma 6 / KWin in a nested `kwin_wayland --virtual`
session — **do not re-litigate it without re-running that experiment**:

- layer-shell **background** layer -> KWin stacks it *below* plasmashell's desktop window,
  which paints opaque, so it is invisible (screenshots byte-identical to the baseline).
- layer-shell **bottom** layer -> stacked *above* the desktop window: visible, but it
  covers the Folder View icons and widgets.
- A wallpaper plugin that paints nothing does **not** make the desktop window
  translucent; forcing that window's opacity to 0.99 through a KWin script let only ~1%
  of the surface underneath bleed through, i.e. the desktop surface is opaque black.

So:

1. `Config::BackendPlasma` — the QML wallpaper plugin in
   `linux/data/plasma/wallpapers/org.wallpaperanim.video` renders *inside* plasmashell,
   which is the only way to be behind the icons on KDE. `PlasmaIntegration` installs it
   into `~/.local/share/plasma/wallpapers` and points the containments at it via
   `org.kde.PlasmaShell.evaluateScript` (`desktops()[i].wallpaperPlugin = ...`).
   **The plugin cannot read `config.json`**: Qt 6 refuses `XMLHttpRequest` on `file://`
   URLs unless `QML_XHR_ALLOW_FILE_READ=1`, which plasmashell does not set, so such a
   plugin renders nothing at all (verified — this is not theoretical). State therefore
   goes through the plugin's own Plasma config via `writeConfig`/`reloadConfig`
   (`PlasmaIntegration::PushState`: `MediaPath`, `FitMode`, `Paused`), and `main.qml`
   re-reads `root.configuration` on a 2 s timer because `KConfigPropertyMap` does not
   reliably emit per-key change signals. `App::ApplyWallpaper` activates the plugin on
   its own when the user applies a wallpaper — startup deliberately does not.
2. `Config::BackendLayerShell` — `WallpaperWindow` (a `QOpenGLWindow` + LayerShellQt) per
   `QScreen`, rendering through libmpv's OpenGL render API. This is the only backend that
   runs GLSL shaders (`ShaderRenderer`).

`Config::BackendAuto` resolves to Plasma on KDE and layer-shell elsewhere
(`App::EffectiveBackend`).

### Layout

```text
linux/src/
├── main.cpp                 # QSurfaceFormat, CLI flags, single-instance socket
├── app.*                    # tray, surfaces, rotation timer, auto-pause, state.json
├── config.*                 # same JSON schema as Windows, XDG paths
├── localization.h           # TR/EN, C++20 designated initializers (not positional!)
├── wallpaper_window.*       # layer-shell surface + libmpv render API
├── shader_renderer.*        # Shadertoy-style GLSL (HLSL is Windows-only)
├── plasma_integration.*     # install/activate/restore the Plasma wallpaper plugin
├── system_monitor.*         # battery via sysfs, "fullscreen" via power-inhibit D-Bus
├── thumbnail.*              # ffmpeg CLI
├── youtube.*                # yt-dlp CLI (no H.264 pin: mpv decodes VP9/AV1 fine)
├── autostart.*              # ~/.config/autostart/wallpaperanim.desktop
└── settings_window.*        # Qt Widgets tabs: library/add/playlists/settings/about
```

Three traps in `PlasmaIntegration` that already cost a debugging round each:
- Never drive the scripting calls off `desktops()`. It returns the containments of the
  *current activity*, not what is painted on the outputs. A session whose current
  activity has gone empty (kactivitymanagerd never wrote `[main]currentActivity`, or an
  activity switch half-failed) hands back the activity's containment with `screen == -1`
  while a different, activity-less containment owns screen 0 — so `wallpaperPlugin = ...`
  succeeds, `IsActive()` says yes, and the desktop never changes. Every script iterates
  `desktopForScreen(0..screenCount-1)` instead (`desktopListPrologue()`), falling back to
  `desktops()` only when that is empty. `IsActive()` likewise requires *every* on-screen
  containment to be ours; "at least one is" made a half-lost desktop read as fine for ever.
- `sourcePluginDir()` must look **executable-relative first**, before `WPA_INSTALL_DATADIR`.
  With the install prefix first, a build-tree run whose prefix happened to be `~/.local`
  found the previously *installed* package, concluded "source == target, already
  installed", and silently never refreshed it.
- Freshness is a **content** comparison, not an mtime one: `QFile::copy` carries the
  source mtime over, so "is the target older?" answers the wrong question after a copy.
- When the package does change, `RestartPlasmaShell()` runs. plasmashell caches compiled
  QML per URL for the life of the process, so replacing files under a running shell
  otherwise keeps the old wallpaper code loaded.

plasmashell re-decides which containment owns a screen on activity changes, monitor
hotplug and its own restarts, and the new containment brings its own `wallpaperplugin` —
so the wallpaper silently reverts while the app keeps running. `App::reassertPlasmaWallpaper()`
(startup, then every ~15 s off `m_monitorTimer`) re-takes the desktop, gated on
`config.plasmaWallpaperActive` (`linuxPlasmaWallpaperActive` in the JSON) so it only ever
reclaims a desktop the user actually handed over — applying a wallpaper or the Activate
button sets it, Restore clears it. Configs predating the flag adopt it from
`PlasmaIntegration::IsConfiguredAnywhere()`, which asks the persisted layout whether *any*
containment (on screen or orphaned) is already pointed at the plugin.

Notes for future changes:
- `Localization::Strings` here uses **designated initializers**, unlike the positional
  Windows lists — append a key to the struct and to both `tr`/`en` blocks.
- The Windows `config.json` writer rewrites the whole document, so it drops the `linux*`
  keys if the same file is used on both systems. That is accepted, not a bug to fix
  silently.
- Auto-pause on Wayland cannot inspect other clients' windows; `IsFullscreenAppActive()`
  is deliberately a power-management-inhibit heuristic.
- There are no automated tests here either. `qmllint` covers the QML plugin;
  verification is running the app.

## Conventions
- Namespaces: `Config`, `Render`, `DesktopIntegration`, `SystemMonitor`, `SystemTray`, `Utils`; WinRT types under `winrt::WallpaperAnimWinUI::implementation`. The Linux port adds `PlasmaIntegration`, `Thumbnail`, `Autostart` and reuses `Config`/`SystemMonitor` names with its own implementations.
- C++20 (`stdcpp20`), C++/WinRT, `NOMINMAX`, `_CRT_SECURE_NO_WARNINGS`. Wallpaper core is native Win32/COM (`ComPtr`); UI is C++/WinRT.
- Long-running work (downloads, update checks) runs on detached `std::thread`s and marshals results back to the UI with `DispatcherQueue().TryEnqueue(...)`.
- Debug logging: `App::LogApp` writes to `debug2.log` and `main.cpp`'s `Log` to `debug.log` (plain `std::ofstream` in the working dir). Both are compiled out unless `_DEBUG` is defined, so Release builds produce no log files.
