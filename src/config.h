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

    struct AppConfig {
        int configVersion = kCurrentConfigVersion;
        std::wstring lastVideoPath;
        int maxFPS = 0; // 0 = match the monitor's refresh rate (smoothest)
        bool pauseOnFullscreen = true;
        bool pauseOnBattery = false;
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
