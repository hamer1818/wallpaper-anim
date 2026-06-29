#include "config.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <windows.h>
#include <shlobj.h>

using json = nlohmann::json;

namespace Config {

    // Helper functions to convert between std::wstring and std::string
    std::string ws2s(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    std::wstring s2ws(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    ConfigManager::ConfigManager() {
        PWSTR path = NULL;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path))) {
            m_configPath = path;
            m_configPath += L"\\WallpaperAnim";
            CreateDirectoryW(m_configPath.c_str(), NULL);
            m_configPath += L"\\config.json";
            CoTaskMemFree(path);
        } else {
            m_configPath = L"config.json"; // Fallback to current dir
        }
    }

    ConfigManager& ConfigManager::GetInstance() {
        static ConfigManager instance;
        return instance;
    }

    void ConfigManager::Load() {
        std::ifstream file(m_configPath);
        if (file.is_open()) {
            json j;
            try {
                file >> j;
                if (j.contains("lastVideoPath")) m_config.lastVideoPath = s2ws(j["lastVideoPath"].get<std::string>());
                if (j.contains("maxFPS")) m_config.maxFPS = j["maxFPS"].get<int>();
                if (j.contains("pauseOnFullscreen")) m_config.pauseOnFullscreen = j["pauseOnFullscreen"].get<bool>();
                if (j.contains("pauseOnBattery")) m_config.pauseOnBattery = j["pauseOnBattery"].get<bool>();
                if (j.contains("isFirstRun")) m_config.isFirstRun = j["isFirstRun"].get<bool>();
                
                if (j.contains("history") && j["history"].is_array()) {
                    for (auto& item : j["history"]) {
                        WallpaperHistoryItem h;
                        h.path = s2ws(item.value("path", ""));
                        h.name = s2ws(item.value("name", ""));
                        h.thumbPath = s2ws(item.value("thumbPath", ""));
                        h.type = item.value("type", 0);
                        m_config.history.push_back(h);
                    }
                }
            } catch (...) {
                // Keep default values on parse error
            }
        }
    }

    void ConfigManager::Save() {
        json j;
        j["lastVideoPath"] = ws2s(m_config.lastVideoPath);
        j["maxFPS"] = m_config.maxFPS;
        j["pauseOnFullscreen"] = m_config.pauseOnFullscreen;
        j["pauseOnBattery"] = m_config.pauseOnBattery;
        j["isFirstRun"] = m_config.isFirstRun;

        json historyArray = json::array();
        for (const auto& h : m_config.history) {
            historyArray.push_back({
                {"path", ws2s(h.path)},
                {"name", ws2s(h.name)},
                {"thumbPath", ws2s(h.thumbPath)},
                {"type", h.type}
            });
        }
        j["history"] = historyArray;

        std::ofstream file(m_configPath);
        if (file.is_open()) {
            file << j.dump(4);
        }
    }
}
