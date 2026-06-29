#pragma once
#include <string>
#include <vector>

namespace Config {
    struct WallpaperHistoryItem {
        std::wstring path;
        std::wstring name;
        std::wstring thumbPath;
        int type; // 0=Video, 1=GIF, 2=Shader, 3=YouTube
    };

    struct AppConfig {
        std::wstring lastVideoPath;
        int maxFPS = 30;
        bool pauseOnFullscreen = true;
        bool pauseOnBattery = false;
        bool isFirstRun = true;
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
