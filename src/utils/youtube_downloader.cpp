#include "youtube_downloader.h"
#include <windows.h>
#include <shlobj.h>
#include <thread>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>

using json = nlohmann::json;

namespace Utils {

    std::wstring YouTubeDownloader::GetCacheDirectory() {
        PWSTR path = NULL;
        std::wstring dir;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path))) {
            dir = path;
            dir += L"\\WallpaperAnim\\YouTube";
            CreateDirectoryW((path + std::wstring(L"\\WallpaperAnim")).c_str(), NULL);
            CreateDirectoryW(dir.c_str(), NULL);
            CoTaskMemFree(path);
        }
        return dir;
    }

    void YouTubeDownloader::FetchResolutionsAsync(const std::string& url, std::function<void(bool, const std::vector<YouTubeResolution>&, const std::string&, const std::string&)> callback) {
        std::thread([url, callback]() {
            // Check if yt-dlp.exe exists
            if (GetFileAttributesA("yt-dlp.exe") == INVALID_FILE_ATTRIBUTES) {
                callback(false, {}, "", "HATA: yt-dlp.exe bulunamadi! Lutfen uygulamanin bulundugu klasorde yt-dlp.exe oldugundan emin olun.");
                return;
            }

            std::string cmd = "yt-dlp.exe -J --no-playlist \"" + url + "\" 2>&1";
            
            FILE* pipe = _popen(cmd.c_str(), "r");
            if (!pipe) {
                callback(false, {}, "", "HATA: yt-dlp.exe baslatilamadi (_popen failed).");
                return;
            }

            std::string result = "";
            char buffer[1024];
            while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                result += buffer;
            }
            int retCode = _pclose(pipe);

            try {
                if (retCode != 0) {
                    throw std::runtime_error("yt-dlp returned error code: " + std::to_string(retCode) + "\n" + result);
                }
                json j = json::parse(result);
                std::string title = j.value("title", "Unknown Title");
                std::vector<YouTubeResolution> resolutions;

                if (j.contains("formats")) {
                    for (auto& f : j["formats"]) {
                        if (f.contains("vcodec") && f["vcodec"] != "none" && f.contains("height") && !f["height"].is_null()) {
                            YouTubeResolution res;
                            res.format_id = f.value("format_id", "");
                            res.height = f.value("height", 0);
                            res.ext = f.value("ext", "mp4");
                            
                            // Only add if not already present or better
                            bool found = false;
                            for (auto& existing : resolutions) {
                                if (existing.height == res.height) {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found && res.height > 0) {
                                resolutions.push_back(res);
                            }
                        }
                    }
                }
                
                // Sort descending by height
                std::sort(resolutions.begin(), resolutions.end(), [](const YouTubeResolution& a, const YouTubeResolution& b) {
                    return a.height > b.height;
                });

                callback(true, resolutions, title, "");
            } catch (const std::exception& e) {
                callback(false, {}, "", "HATA: JSON parcalama basarisiz veya video bulunamadi.\nDetay:\n" + std::string(e.what()));
            } catch (...) {
                callback(false, {}, "", "HATA: Bilinmeyen bir hata olustu.\nCikti:\n" + result);
            }
        }).detach();
    }

    void YouTubeDownloader::DownloadAsync(const std::string& url, const std::string& format_id, std::atomic<float>* progress, std::function<void(bool, const std::wstring&, const std::string&)> callback) {
        std::thread([url, format_id, progress, callback]() {
            std::wstring cacheDir = GetCacheDirectory();
            char cacheDirMbs[512];
            WideCharToMultiByte(CP_UTF8, 0, cacheDir.c_str(), -1, cacheDirMbs, sizeof(cacheDirMbs), NULL, NULL);

            if (GetFileAttributesA("yt-dlp.exe") == INVALID_FILE_ATTRIBUTES) {
                callback(false, L"", "HATA: yt-dlp.exe bulunamadi!");
                return;
            }

            // yt-dlp will merge bestaudio automatically if we ask for it.
            // But since we want to output mp4, we use --merge-output-format mp4
            std::string cmd = "yt-dlp.exe -f \"" + format_id + "+bestaudio/best\" --merge-output-format mp4 -o \"" + std::string(cacheDirMbs) + "\\%(id)s.%(ext)s\" --newline \"" + url + "\" 2>&1";
            
            FILE* pipe = _popen(cmd.c_str(), "r");
            if (!pipe) {
                callback(false, L"", "HATA: yt-dlp.exe baslatilamadi.");
                return;
            }

            std::regex progRegex(R"(\[download\]\s+([0-9.]+)%)");
            std::string result = "";
            char buffer[1024];
            while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                std::string line = buffer;
                std::smatch match;
                if (std::regex_search(line, match, progRegex)) {
                    if (match.size() > 1 && progress) {
                        float pct = std::stof(match[1].str());
                        *progress = pct / 100.0f;
                    }
                }
                result += line;
            }
            int retCode = _pclose(pipe);

            if (retCode == 0) {
                // To get the final filename, we can parse it from yt-dlp output or just run a quick -O to get filename
                std::string nameCmd = "yt-dlp.exe --get-filename -o \"" + std::string(cacheDirMbs) + "\\%(id)s.mp4\" \"" + url + "\"";
                FILE* np = _popen(nameCmd.c_str(), "r");
                char nBuf[1024];
                std::string finalPath = "";
                if (np && fgets(nBuf, sizeof(nBuf), np) != NULL) {
                    finalPath = nBuf;
                    // trim newline
                    finalPath.erase(finalPath.find_last_not_of(" \n\r\t") + 1);
                }
                if (np) _pclose(np);

                int size_needed = MultiByteToWideChar(CP_UTF8, 0, &finalPath[0], (int)finalPath.size(), NULL, 0);
                std::wstring wPath(size_needed, 0);
                MultiByteToWideChar(CP_UTF8, 0, &finalPath[0], (int)finalPath.size(), &wPath[0], size_needed);
                
                callback(true, wPath, "");
            } else {
                callback(false, L"", "HATA: Video indirilirken hata olustu. Hata Kodu: " + std::to_string(retCode) + "\nCikti:\n" + result);
            }
        }).detach();
    }
}
