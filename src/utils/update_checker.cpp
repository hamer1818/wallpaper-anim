#include "update_checker.h"
#include "../version.h"
#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <nlohmann/json.hpp>

#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;

namespace Utils {

    static std::string PerformHttpRequest(const std::wstring& url) {
        std::string responseData;
        HINTERNET hSession = WinHttpOpen(L"WallpaperAnim Update Checker/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return "";

        HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/repos/hamer1818/wallpaper-anim/releases/latest",
                                                    NULL, WINHTTP_NO_REFERER,
                                                    WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                    WINHTTP_FLAG_SECURE);
            if (hRequest) {
                // GitHub API requires a User-Agent
                WinHttpAddRequestHeaders(hRequest, L"User-Agent: WallpaperAnim\r\n", -1, WINHTTP_ADDREQ_FLAG_ADD);

                if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                    if (WinHttpReceiveResponse(hRequest, NULL)) {
                        DWORD dwSize = 0;
                        DWORD dwDownloaded = 0;
                        do {
                            dwSize = 0;
                            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                                break;
                            }
                            if (dwSize == 0) break;
                            
                            char* pszOutBuffer = new char[dwSize + 1];
                            if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
                                pszOutBuffer[dwDownloaded] = '\0';
                                responseData += pszOutBuffer;
                            }
                            delete[] pszOutBuffer;
                        } while (dwSize > 0);
                    }
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
        return responseData;
    }

    // Helper to parse version string "v1.2.3" or "1.2.3" into major, minor, patch
    static bool ParseVersion(const std::string& verStr, int& major, int& minor, int& patch) {
        const char* p = verStr.c_str();
        if (*p == 'v' || *p == 'V') p++;
        if (sscanf_s(p, "%d.%d.%d", &major, &minor, &patch) == 3) {
            return true;
        }
        return false;
    }

    void UpdateChecker::CheckForUpdateAsync(std::function<void(const UpdateInfo&)> callback) {
        std::thread([callback]() {
            UpdateInfo info = {false, "", "", ""};
            
            std::string response = PerformHttpRequest(L"https://api.github.com/repos/hamer1818/wallpaper-anim/releases/latest");
            if (response.empty()) {
                info.errorMessage = "Internet baglantisi kurulamadi veya sunucuya erisilemedi.";
                callback(info);
                return;
            }

            try {
                json j = json::parse(response);
                if (j.contains("message") && j["message"] == "Not Found") {
                    info.errorMessage = "Release bulunamadi (henuz yayinlanmamis olabilir).";
                    callback(info);
                    return;
                }

                std::string tag_name = j.value("tag_name", "");
                std::string html_url = j.value("html_url", "");

                if (tag_name.empty()) {
                    info.errorMessage = "Surum bilgisi alinamadi.";
                    callback(info);
                    return;
                }

                int remoteMajor = 0, remoteMinor = 0, remotePatch = 0;
                if (ParseVersion(tag_name, remoteMajor, remoteMinor, remotePatch)) {
                    if (remoteMajor > APP_VERSION_MAJOR ||
                       (remoteMajor == APP_VERSION_MAJOR && remoteMinor > APP_VERSION_MINOR) ||
                       (remoteMajor == APP_VERSION_MAJOR && remoteMinor == APP_VERSION_MINOR && remotePatch > APP_VERSION_PATCH)) {
                        
                        info.hasUpdate = true;
                        info.version = tag_name;
                        info.releaseUrl = html_url;
                    } else {
                        info.hasUpdate = false;
                    }
                } else {
                    info.errorMessage = "Surum formati anlasilamadi: " + tag_name;
                }
                
                callback(info);
            } catch (const std::exception& e) {
                info.errorMessage = std::string("Hata olustu: ") + e.what();
                callback(info);
            }
        }).detach();
    }
}
