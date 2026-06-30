#include "youtube_downloader.h"
#include <windows.h>
#include <shlobj.h>
#include <thread>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>

using json = nlohmann::json;

namespace {
    struct CmdResult {
        int exitCode;
        std::string output;
    };

    // Resolve the directory the running executable lives in (NOT the current
    // working directory, which may be System32 when launched from a shortcut
    // or from the Windows autostart registry key).
    std::wstring GetExeDirW() {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        wchar_t* lastSlash = wcsrchr(path, L'\\');
        if (lastSlash) *(lastSlash + 1) = L'\0';
        return path;
    }

    std::wstring GetYtDlpPathW() {
        return GetExeDirW() + L"yt-dlp.exe";
    }

    // ANSI (CP_ACP) narrow path, matching what CreateProcessA expects.
    std::string ToAnsi(const std::wstring& ws) {
        int len = WideCharToMultiByte(CP_ACP, 0, ws.c_str(), -1, NULL, 0, NULL, NULL);
        if (len <= 0) return "";
        std::string s(len - 1, '\0');
        WideCharToMultiByte(CP_ACP, 0, ws.c_str(), -1, &s[0], len, NULL, NULL);
        return s;
    }

    std::string GetYtDlpPathA() {
        return ToAnsi(GetYtDlpPathW());
    }

    CmdResult RunCommandHidden(const std::string& cmd, std::function<void(const std::string&)> onLine = nullptr) {
        CmdResult res = { -1, "" };
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;
        
        HANDLE hRead, hWrite;
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return res;
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));
        
        std::string mutableCmd = cmd;
        if (!CreateProcessA(NULL, &mutableCmd[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            CloseHandle(hRead);
            CloseHandle(hWrite);
            return res;
        }
        CloseHandle(hWrite);

        char buffer[1024];
        DWORD bytesRead;
        std::string currentLine = "";
        while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            res.output += buffer;
            if (onLine) {
                for (DWORD i = 0; i < bytesRead; ++i) {
                    if (buffer[i] == '\n') {
                        onLine(currentLine);
                        currentLine.clear();
                    } else if (buffer[i] != '\r') {
                        currentLine += buffer[i];
                    }
                }
            }
        }
        if (onLine && !currentLine.empty()) {
            onLine(currentLine);
        }
        
        CloseHandle(hRead);
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        res.exitCode = exitCode;
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        return res;
    }
}

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
            // Check if yt-dlp.exe exists next to the executable
            if (GetFileAttributesW(GetYtDlpPathW().c_str()) == INVALID_FILE_ATTRIBUTES) {
                callback(false, {}, "", "HATA: yt-dlp.exe bulunamadi! Lutfen uygulamanin bulundugu klasorde yt-dlp.exe oldugundan emin olun.");
                return;
            }

            std::string cmd = "\"" + GetYtDlpPathA() + "\" -J --no-playlist \"" + url + "\"";
            
            CmdResult execRes = RunCommandHidden(cmd);
            if (execRes.exitCode == -1 && execRes.output.empty()) {
                callback(false, {}, "", "HATA: yt-dlp.exe baslatilamadi (CreateProcess failed).");
                return;
            }

            int retCode = execRes.exitCode;
            std::string result = execRes.output;

            try {
                if (retCode != 0) {
                    throw std::runtime_error("yt-dlp returned error code: " + std::to_string(retCode) + "\n" + result);
                }

                // yt-dlp can sometimes print WARNING messages to stdout before the JSON.
                // We find the first '{' to ensure we only parse the JSON part.
                size_t jsonStart = result.find('{');
                if (jsonStart != std::string::npos) {
                    result = result.substr(jsonStart);
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
            // Use CP_ACP (not CP_UTF8): the command line is passed to CreateProcessA,
            // so the path must be in the system ANSI codepage. The cache dir lives
            // under %LocalAppData%, which contains the (possibly non-ASCII) user name.
            std::string cacheDirMbs = ToAnsi(cacheDir);

            if (GetFileAttributesW(GetYtDlpPathW().c_str()) == INVALID_FILE_ATTRIBUTES) {
                callback(false, L"", "HATA: yt-dlp.exe bulunamadi!");
                return;
            }

            std::string ytDlp = GetYtDlpPathA();

            // yt-dlp will merge bestaudio automatically if we ask for it.
            // But since we want to output mp4, we use --merge-output-format mp4
            std::string cmd = "\"" + ytDlp + "\" -f \"" + format_id + "+bestaudio/best\" --merge-output-format mp4 -o \"" + cacheDirMbs + "\\%(id)s.%(ext)s\" --newline \"" + url + "\"";
            
            std::regex progRegex(R"(\[download\]\s+([0-9.]+)%)");
            CmdResult execRes = RunCommandHidden(cmd, [&](const std::string& line) {
                std::smatch match;
                if (std::regex_search(line, match, progRegex)) {
                    if (match.size() > 1 && progress) {
                        float pct = std::stof(match[1].str());
                        *progress = pct / 100.0f;
                    }
                }
            });

            if (execRes.exitCode == -1 && execRes.output.empty()) {
                callback(false, L"", "HATA: yt-dlp.exe baslatilamadi.");
                return;
            }

            int retCode = execRes.exitCode;
            std::string result = execRes.output;

            if (retCode == 0) {
                // To get the final filename, we can parse it from yt-dlp output or just run a quick -O to get filename
                std::string nameCmd = "\"" + ytDlp + "\" --get-filename --no-warnings -o \"" + cacheDirMbs + "\\%(id)s.mp4\" \"" + url + "\"";
                CmdResult nameRes = RunCommandHidden(nameCmd);
                std::string finalPath = nameRes.output;

                if (!finalPath.empty()) {
                    finalPath.erase(finalPath.find_last_not_of(" \n\r\t") + 1);
                }

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
