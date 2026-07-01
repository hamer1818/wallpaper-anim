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

## Conventions
- Namespaces: `Config`, `Render`, `DesktopIntegration`, `SystemMonitor`, `SystemTray`, `Utils`; WinRT types under `winrt::WallpaperAnimWinUI::implementation`.
- C++20 (`stdcpp20`), C++/WinRT, `NOMINMAX`, `_CRT_SECURE_NO_WARNINGS`. Wallpaper core is native Win32/COM (`ComPtr`); UI is C++/WinRT.
- Long-running work (downloads, update checks) runs on detached `std::thread`s and marshals results back to the UI with `DispatcherQueue().TryEnqueue(...)`.
- Debug logging: `App::LogApp` writes to `debug2.log` and `main.cpp`'s `Log` to `debug.log` (plain `std::ofstream` in the working dir). Both are compiled out unless `_DEBUG` is defined, so Release builds produce no log files.
