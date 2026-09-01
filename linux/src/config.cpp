#include "config.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace Config {

    namespace {

        std::string EnvOr(const char* name, const std::string& fallback) {
            const char* v = std::getenv(name);
            if (v && *v) return std::string(v);
            return fallback;
        }

        std::string HomeDir() {
            return EnvOr("HOME", "/tmp");
        }

        std::string EnsureDir(const std::string& path) {
            std::error_code ec;
            fs::create_directories(path, ec);
            return path;
        }

        std::string LowerExtension(const std::string& path) {
            const auto dot = path.find_last_of('.');
            if (dot == std::string::npos) return {};
            std::string ext = path.substr(dot + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return ext;
        }

    } // namespace

    std::string ConfigDir() {
        const std::string base = EnvOr("XDG_CONFIG_HOME", HomeDir() + "/.config");
        return EnsureDir(base + "/WallpaperAnim");
    }

    std::string DataDir() {
        const std::string base = EnvOr("XDG_DATA_HOME", HomeDir() + "/.local/share");
        return EnsureDir(base + "/WallpaperAnim");
    }

    std::string ThumbsDir() { return EnsureDir(DataDir() + "/thumbnails"); }
    std::string DownloadsDir() { return EnsureDir(DataDir() + "/downloads"); }

    bool IsShaderPath(const std::string& path) {
        const std::string ext = LowerExtension(path);
        return ext == "glsl" || ext == "frag" || ext == "fs" || ext == "hlsl";
    }

    int MediaTypeForPath(const std::string& path) {
        const std::string ext = LowerExtension(path);
        if (ext == "gif") return 1;
        if (IsShaderPath(path)) return 2;
        return 0;
    }

    ConfigManager::ConfigManager() {
        m_configPath = ConfigDir() + "/config.json";
    }

    ConfigManager& ConfigManager::GetInstance() {
        static ConfigManager instance;
        return instance;
    }

    void ConfigManager::Load() {
        std::ifstream file(m_configPath);
        if (!file.is_open()) return;

        json j;
        try {
            file >> j;
        } catch (...) {
            return; // Keep defaults on a corrupt file rather than wiping it.
        }

        try {
            // Absent means a pre-versioning (v0) config; future breaking layout changes
            // branch on this value before reading fields.
            m_config.configVersion = j.value("configVersion", 0);
            m_config.lastVideoPath = j.value("lastVideoPath", "");
            m_config.maxFPS = j.value("maxFPS", 0);
            m_config.fitMode = j.value("fitMode", 0);
            m_config.pauseOnFullscreen = j.value("pauseOnFullscreen", false);
            m_config.pauseOnBattery = j.value("pauseOnBattery", false);
            m_config.playlistEnabled = j.value("playlistEnabled", false);
            m_config.playlistIntervalMin = j.value("playlistIntervalMin", 30);
            m_config.playlistShuffle = j.value("playlistShuffle", false);
            m_config.activePlaylist = j.value("activePlaylist", "");

            m_config.playlists.clear();
            if (j.contains("playlists") && j["playlists"].is_array()) {
                for (const auto& pl : j["playlists"]) {
                    Playlist p;
                    p.name = pl.value("name", "");
                    if (pl.contains("paths") && pl["paths"].is_array()) {
                        for (const auto& pp : pl["paths"]) {
                            if (pp.is_string()) p.paths.push_back(pp.get<std::string>());
                        }
                    }
                    if (!p.name.empty()) m_config.playlists.push_back(std::move(p));
                }
            }

            m_config.isFirstRun = j.value("isFirstRun", true);
            m_config.hideMinimizeWarning = j.value("hideMinimizeWarning", false);
            m_config.lastUpdateCheck = j.value("lastUpdateCheck", static_cast<int64_t>(0));
            m_config.language = j.value("language", "");

            m_config.history.clear();
            if (j.contains("history") && j["history"].is_array()) {
                for (const auto& item : j["history"]) {
                    WallpaperHistoryItem h;
                    h.path = item.value("path", "");
                    h.name = item.value("name", "");
                    h.thumbPath = item.value("thumbPath", "");
                    h.type = item.value("type", 0);
                    if (!h.path.empty()) m_config.history.push_back(std::move(h));
                }
            }

            // Linux-only keys.
            m_config.backend = j.value("linuxBackend", static_cast<int>(BackendAuto));
            m_config.layer = j.value("linuxLayer", static_cast<int>(LayerBackground));
            m_config.pauseWhenHidden = j.value("linuxPauseWhenHidden", true);
            m_config.hardwareDecode = j.value("linuxHardwareDecode", true);
            m_config.volume = j.value("linuxVolume", 0);
            m_config.plasmaWallpaperActive = j.value("linuxPlasmaWallpaperActive", false);

            // Clamp anything an older/edited config may have overgrown.
            if (m_config.history.size() > kMaxHistoryItems) {
                m_config.history.resize(kMaxHistoryItems);
            }
            m_config.configVersion = kCurrentConfigVersion;
        } catch (...) {
            // Keep whatever was parsed successfully.
        }
    }

    void ConfigManager::Save() {
        // Enforce the history cap centrally so every path that adds items is covered.
        if (m_config.history.size() > kMaxHistoryItems) {
            m_config.history.resize(kMaxHistoryItems);
        }

        json j;
        j["configVersion"] = kCurrentConfigVersion;
        j["lastVideoPath"] = m_config.lastVideoPath;
        j["maxFPS"] = m_config.maxFPS;
        j["fitMode"] = m_config.fitMode;
        j["pauseOnFullscreen"] = m_config.pauseOnFullscreen;
        j["pauseOnBattery"] = m_config.pauseOnBattery;
        j["playlistEnabled"] = m_config.playlistEnabled;
        j["playlistIntervalMin"] = m_config.playlistIntervalMin;
        j["playlistShuffle"] = m_config.playlistShuffle;
        j["activePlaylist"] = m_config.activePlaylist;

        json plArray = json::array();
        for (const auto& p : m_config.playlists) {
            json pj;
            pj["name"] = p.name;
            pj["paths"] = p.paths;
            plArray.push_back(std::move(pj));
        }
        j["playlists"] = std::move(plArray);

        j["isFirstRun"] = m_config.isFirstRun;
        j["hideMinimizeWarning"] = m_config.hideMinimizeWarning;
        j["lastUpdateCheck"] = m_config.lastUpdateCheck;
        j["language"] = m_config.language;

        json historyArray = json::array();
        for (const auto& h : m_config.history) {
            historyArray.push_back({
                {"path", h.path},
                {"name", h.name},
                {"thumbPath", h.thumbPath},
                {"type", h.type}
            });
        }
        j["history"] = std::move(historyArray);

        j["linuxBackend"] = m_config.backend;
        j["linuxLayer"] = m_config.layer;
        j["linuxPauseWhenHidden"] = m_config.pauseWhenHidden;
        j["linuxHardwareDecode"] = m_config.hardwareDecode;
        j["linuxVolume"] = m_config.volume;
        j["linuxPlasmaWallpaperActive"] = m_config.plasmaWallpaperActive;

        // Write through a temp file: the Plasma wallpaper plugin polls this file and
        // must never observe a half-written document.
        const std::string tmpPath = m_configPath + ".tmp";
        {
            std::ofstream file(tmpPath, std::ios::trunc);
            if (!file.is_open()) return;
            file << j.dump(4);
            if (!file.good()) return;
        }
        std::error_code ec;
        fs::rename(tmpPath, m_configPath, ec);
        if (ec) fs::remove(tmpPath, ec);
    }
}
