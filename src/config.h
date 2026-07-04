#pragma once
#include <string>
#include <vector>

namespace Config {
    // Current on-disk config schema version. Bump when the JSON layout changes and
    // add migration handling in ConfigManager::Load.
    constexpr int kCurrentConfigVersion = 1;
    // Cap the wallpaper history so the config file and thumbnail cache cannot grow forever.
    constexpr size_t kMaxHistoryItems = 50;

    struct WallpaperHistoryItem {
        std::wstring path;
        std::wstring name;
        std::wstring thumbPath;
        int type; // 0=Video, 1=GIF, 2=Shader, 3=YouTube
    };

    // A named collection of wallpapers (by path) used for scoped auto-rotation.
    struct Playlist {
        std::wstring name;
        std::vector<std::wstring> paths;
    };

    struct AppConfig {
        int configVersion = kCurrentConfigVersion;
        std::wstring lastVideoPath;
        int maxFPS = 0; // 0 = match the monitor's refresh rate (smoothest)
        // How the media is fitted to each monitor when its aspect ratio differs:
        // 0 = Fill (preserve aspect, crop overflow), 1 = Fit (preserve aspect, letterbox),
        // 2 = Stretch (fill exactly, may distort), 3 = Center (native pixels, centered).
        int fitMode = 0;
        bool pauseOnFullscreen = true;
        bool pauseOnBattery = false;
        // Auto-rotation: cycle through the library on a timer.
        bool playlistEnabled = false;
        int playlistIntervalMin = 30; // minutes between switches
        bool playlistShuffle = false;
        // Named playlists and the currently selected one (empty = whole library).
        // When set, auto-rotation cycles only through that playlist's items.
        std::vector<Playlist> playlists;
        std::wstring activePlaylist;
        bool isFirstRun = true;
        bool hideMinimizeWarning = false;
        int64_t lastUpdateCheck = 0;
        std::wstring language = L""; // Empty means auto-detect
        std::vector<WallpaperHistoryItem> history;
    };

    class ConfigManager {
    public:
        static ConfigManager& GetInstance();

        void Load();
        void Save();

        const AppConfig& GetConfig() const { return m_config; }
        AppConfig& GetConfig() { return m_config; }

    private:
        ConfigManager();
        ~ConfigManager() = default;

        AppConfig m_config;
        std::wstring m_configPath;
    };
}
