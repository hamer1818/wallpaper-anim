#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Linux port of the Windows Config module. The on-disk JSON schema is deliberately
// identical to the Windows build's %LocalAppData%\WallpaperAnim\config.json so a
// config can be carried between the two; strings are held as UTF-8 std::string here
// instead of std::wstring, and a handful of Linux-only keys are appended.
namespace Config {

    constexpr int kCurrentConfigVersion = 1;
    constexpr size_t kMaxHistoryItems = 50;

    struct WallpaperHistoryItem {
        std::string path;
        std::string name;
        std::string thumbPath;
        int type = 0; // 0=Video, 1=GIF, 2=Shader, 3=YouTube
    };

    // A named collection of wallpapers (by path) used for scoped auto-rotation.
    struct Playlist {
        std::string name;
        std::vector<std::string> paths;
    };

    // How the wallpaper reaches the screen. On Plasma the compositor keeps every
    // layer-shell background surface underneath plasmashell's opaque desktop window,
    // so drawing behind the desktop icons is only possible from inside plasmashell
    // itself - hence the wallpaper-plugin backend.
    enum Backend {
        BackendAuto = 0,      // Plasma on KDE, layer-shell everywhere else
        BackendPlasma = 1,    // KDE Plasma wallpaper plugin (renders behind icons)
        BackendLayerShell = 2 // wlr-layer-shell surface (Hyprland, Sway, wlroots, ...)
    };

    // Which wlr-layer-shell layer the surface is placed on (layer-shell backend only).
    enum LayerChoice {
        LayerBackground = 0, // below everything, including the Plasma desktop
        LayerBottom = 1      // above the desktop, below normal windows
    };

    struct AppConfig {
        int configVersion = kCurrentConfigVersion;
        std::string lastVideoPath;
        int maxFPS = 0; // 0 = follow the source / monitor refresh rate
        // 0 = Fill (preserve aspect, crop overflow), 1 = Fit (letterbox),
        // 2 = Stretch (fill exactly, may distort), 3 = Center (native pixels).
        int fitMode = 0;
        // Off by default on Linux: see SystemMonitor::IsFullscreenAppActive - the only
        // portable Wayland signal is a screensaver/power inhibition, which browsers and
        // remote-desktop tools hold for hours. pauseWhenHidden is the accurate one.
        bool pauseOnFullscreen = false;
        bool pauseOnBattery = false;
        bool playlistEnabled = false;
        int playlistIntervalMin = 30;
        bool playlistShuffle = false;
        std::vector<Playlist> playlists;
        std::string activePlaylist;
        bool isFirstRun = true;
        bool hideMinimizeWarning = false;
        int64_t lastUpdateCheck = 0;
        std::string language; // "tr", "en", empty = auto-detect
        std::vector<WallpaperHistoryItem> history;

        // --- Linux-only settings (ignored by the Windows build) ---
        int backend = BackendAuto;
        int layer = LayerBackground;
        // Pause decoding while the compositor stops asking for frames (the wallpaper
        // is fully covered). Cheap, compositor-agnostic replacement for the Win32
        // "is a fullscreen app in front?" check.
        bool pauseWhenHidden = true;
        bool hardwareDecode = true;
        int volume = 0; // 0 = silent; the wallpaper is muted by default
        // Set once the user hands the Plasma desktop to us (applying a wallpaper, or
        // the Activate button). It is what licenses App to re-take the containment
        // later - plasmashell drops the wallpaper plugin on its own when it rebinds a
        // desktop - without ever hijacking a desktop the user never offered.
        bool plasmaWallpaperActive = false;
    };

    class ConfigManager {
    public:
        static ConfigManager& GetInstance();

        void Load();
        void Save();

        const AppConfig& GetConfig() const { return m_config; }
        AppConfig& GetConfig() { return m_config; }

        const std::string& Path() const { return m_configPath; }

    private:
        ConfigManager();
        ~ConfigManager() = default;

        AppConfig m_config;
        std::string m_configPath;
    };

    // XDG locations, created on first use.
    std::string ConfigDir();    // ~/.config/WallpaperAnim
    std::string DataDir();      // ~/.local/share/WallpaperAnim
    std::string ThumbsDir();    // <data>/thumbnails
    std::string DownloadsDir(); // <data>/downloads

    // 0=Video, 1=GIF, 2=Shader, 3=YouTube-download, based on the file extension.
    int MediaTypeForPath(const std::string& path);
    bool IsShaderPath(const std::string& path);
}
