#include "update_checker.h"
#include "../version.h"
#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <nlohmann/json.hpp>
#include <shlobj.h>
#include <shlwapi.h>
#include <fstream>
#include <vector>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

using json = nlohmann::json;

namespace Utils {

    // --------------------------------------------------------------------------
    // HTTP helpers
    // --------------------------------------------------------------------------

    struct ParsedUrl {
        std::wstring host;
        std::wstring path;
        INTERNET_PORT port;
        bool isHttps;
    };

    static bool ParseUrl(const std::string& url, ParsedUrl& out) {
        std::wstring wurl(url.begin(), url.end());
        URL_COMPONENTS uc = {};
        uc.dwStructSize = sizeof(uc);
        wchar_t hostBuf[256] = {};
        wchar_t pathBuf[2048] = {};
        uc.lpszHostName = hostBuf;
        uc.dwHostNameLength = _countof(hostBuf);
        uc.lpszUrlPath = pathBuf;
        uc.dwUrlPathLength = _countof(pathBuf);

        if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc))
            return false;

        out.host = hostBuf;
        out.path = pathBuf;
        out.port = uc.nPort;
        out.isHttps = (uc.nScheme == INTERNET_SCHEME_HTTPS);
        return true;
    }

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

    // --------------------------------------------------------------------------
    // Download file via WinHTTP (follows redirects, reports progress)
    // --------------------------------------------------------------------------

    static bool DownloadFileToPath(const std::string& url, const std::wstring& destPath,
                                   std::atomic<float>* progress, std::string& outError) {
        if (progress) progress->store(0.0f);

        // GitHub download URLs redirect; we need to follow them.
        // WinHTTP follows redirects by default.
        ParsedUrl parsed;
        if (!ParseUrl(url, parsed)) {
            outError = "URL ayriştirilamadi.";
            return false;
        }

        HINTERNET hSession = WinHttpOpen(L"WallpaperAnim Updater/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            outError = "HTTP oturumu olusturulamadi.";
            return false;
        }

        HINTERNET hConnect = WinHttpConnect(hSession, parsed.host.c_str(), parsed.port, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            outError = "Sunucuya baglanılamadi.";
            return false;
        }

        DWORD flags = parsed.isHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", parsed.path.c_str(),
                                                NULL, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            outError = "HTTP istegi olusturulamadi.";
            return false;
        }

        WinHttpAddRequestHeaders(hRequest, L"User-Agent: WallpaperAnim\r\n", -1, WINHTTP_ADDREQ_FLAG_ADD);

        // Enable automatic redirect following
        DWORD optionFlags = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &optionFlags, sizeof(optionFlags));

        bool success = false;
        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, NULL)) {

            // Get content length for progress
            DWORD contentLength = 0;
            DWORD bufferLen = sizeof(contentLength);
            DWORD headerIndex = WINHTTP_NO_HEADER_INDEX;
            wchar_t clBuf[32] = {};
            DWORD clBufLen = sizeof(clBuf);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, clBuf, &clBufLen, &headerIndex)) {
                contentLength = (DWORD)_wtoi(clBuf);
            }

            std::ofstream outFile(destPath, std::ios::binary);
            if (!outFile.is_open()) {
                outError = "Dosya olusturulamadi: indirme hedefi.";
            } else {
                DWORD totalRead = 0;
                DWORD dwSize = 0;
                DWORD dwDownloaded = 0;
                char buffer[65536];

                do {
                    dwSize = 0;
                    if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                    if (dwSize == 0) break;

                    DWORD toRead = std::min(dwSize, (DWORD)sizeof(buffer));
                    if (WinHttpReadData(hRequest, buffer, toRead, &dwDownloaded)) {
                        outFile.write(buffer, dwDownloaded);
                        totalRead += dwDownloaded;
                        if (progress && contentLength > 0) {
                            progress->store((float)totalRead / (float)contentLength);
                        }
                    } else {
                        break;
                    }
                } while (dwSize > 0);

                outFile.close();
                if (totalRead > 0) {
                    success = true;
                    if (progress) progress->store(1.0f);
                } else {
                    outError = "Dosya indirilemedi (0 byte).";
                }
            }
        } else {
            outError = "HTTP istegi gonderilemedi veya yanit alinamadi.";
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return success;
    }

    // --------------------------------------------------------------------------
    // Extract ZIP using Windows Shell (IShellDispatch)
    // --------------------------------------------------------------------------

    static bool ExtractZipToFolder(const std::wstring& zipPath, const std::wstring& destFolder, std::string& outError) {
        // Create destination folder if it doesn't exist
        CreateDirectoryW(destFolder.c_str(), NULL);

        CoInitialize(NULL);

        IShellDispatch* pShellDispatch = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER,
                                      IID_IShellDispatch, (void**)&pShellDispatch);
        if (FAILED(hr) || !pShellDispatch) {
            CoUninitialize();
            outError = "Shell nesnesi olusturulamadi.";
            return false;
        }

        VARIANT vZip, vDest;
        VariantInit(&vZip);
        VariantInit(&vDest);
        vZip.vt = VT_BSTR;
        vZip.bstrVal = SysAllocString(zipPath.c_str());
        vDest.vt = VT_BSTR;
        vDest.bstrVal = SysAllocString(destFolder.c_str());

        Folder* pZipFolder = nullptr;
        Folder* pDestFolder = nullptr;
        bool success = false;

        hr = pShellDispatch->NameSpace(vZip, &pZipFolder);
        if (SUCCEEDED(hr) && pZipFolder) {
            hr = pShellDispatch->NameSpace(vDest, &pDestFolder);
            if (SUCCEEDED(hr) && pDestFolder) {
                FolderItems* pItems = nullptr;
                pZipFolder->Items(&pItems);
                if (pItems) {
                    VARIANT vItems;
                    VariantInit(&vItems);
                    vItems.vt = VT_DISPATCH;
                    vItems.pdispVal = pItems;

                    // Options: 4=no dialog, 16=yes to all, 256=no error UI, 512=no confirm filename, 1024=no confirm dir
                    VARIANT vOptions;
                    VariantInit(&vOptions);
                    vOptions.vt = VT_I4;
                    vOptions.lVal = 4 | 16 | 256 | 512 | 1024;

                    hr = pDestFolder->CopyHere(vItems, vOptions);
                    success = SUCCEEDED(hr);
                    if (!success) {
                        outError = "ZIP dosyasi acilamadi.";
                    }
                    pItems->Release();
                }
                pDestFolder->Release();
            } else {
                outError = "Hedef klasor acilamadi.";
            }
            pZipFolder->Release();
        } else {
            outError = "ZIP dosyasi acilamadi.";
        }

        SysFreeString(vZip.bstrVal);
        SysFreeString(vDest.bstrVal);
        pShellDispatch->Release();
        CoUninitialize();

        // CopyHere is async internally - wait a bit for files to be extracted
        if (success) {
            Sleep(2000);
        }

        return success;
    }

    // --------------------------------------------------------------------------
    // Create batch updater script
    // --------------------------------------------------------------------------

    static std::wstring GetExeDir() {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        PathRemoveFileSpecW(path);
        return path;
    }

    static std::wstring GetExePath() {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        return path;
    }

    static bool CreateAndLaunchUpdater(const std::wstring& extractedFolder, std::string& outError) {
        std::wstring exeDir = GetExeDir();
        std::wstring exePath = GetExePath();
        DWORD pid = GetCurrentProcessId();

        // Find the actual content folder inside extracted folder.
        // The ZIP may contain files directly or inside a subfolder.
        // We look for WallpaperAnimWinUI.exe to determine the source directory.
        std::wstring sourceDir = extractedFolder;

        // Check if exe is directly in extractedFolder
        std::wstring testExe = extractedFolder + L"\\WallpaperAnimWinUI.exe";
        if (GetFileAttributesW(testExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
            // Search one level of subdirectories
            WIN32_FIND_DATAW fd;
            std::wstring searchPattern = extractedFolder + L"\\*";
            HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                        wcscmp(fd.cFileName, L".") != 0 &&
                        wcscmp(fd.cFileName, L"..") != 0) {
                        std::wstring subTest = extractedFolder + L"\\" + fd.cFileName + L"\\WallpaperAnimWinUI.exe";
                        if (GetFileAttributesW(subTest.c_str()) != INVALID_FILE_ATTRIBUTES) {
                            sourceDir = extractedFolder + L"\\" + fd.cFileName;
                            break;
                        }
                    }
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            }
        }

        // Build batch script path
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        std::wstring batPath = std::wstring(tempPath) + L"wallpaper_updater.bat";

        // Convert wstrings to narrow for the batch file
        // Use the wide paths directly in the batch with proper quoting
        char pidStr[16];
        sprintf_s(pidStr, "%lu", pid);

        // Write batch file (UTF-8/ANSI batch)
        std::ofstream bat(batPath);
        if (!bat.is_open()) {
            outError = "Guncelleme script'i olusturulamadi.";
            return false;
        }

        // Convert wide strings to narrow for batch file
        auto wtoNarrow = [](const std::wstring& ws) -> std::string {
            int len = WideCharToMultiByte(CP_ACP, 0, ws.c_str(), -1, NULL, 0, NULL, NULL);
            std::string s(len - 1, '\0');
            WideCharToMultiByte(CP_ACP, 0, ws.c_str(), -1, &s[0], len, NULL, NULL);
            return s;
        };

        std::string narrowSourceDir = wtoNarrow(sourceDir);
        std::string narrowExeDir = wtoNarrow(exeDir);
        std::string narrowExePath = wtoNarrow(exePath);
        std::string narrowExtractedFolder = wtoNarrow(extractedFolder);
        std::string narrowZipPath = wtoNarrow(std::wstring(tempPath) + L"WallpaperAnim_update.zip");
        std::string narrowBatPath = wtoNarrow(batPath);

        bat << "@echo off" << std::endl;
        bat << "chcp 65001 >nul 2>&1" << std::endl;
        bat << "echo WallpaperAnim guncelleniyor..." << std::endl;
        bat << "echo Uygulamanin kapanmasi bekleniyor..." << std::endl;
        bat << std::endl;
        bat << "REM Wait for the application to close" << std::endl;
        bat << "taskkill /PID " << pidStr << " /F >nul 2>&1" << std::endl;
        bat << "timeout /t 3 /nobreak >nul" << std::endl;
        bat << std::endl;
        bat << "REM Copy new files over old ones" << std::endl;
        bat << "echo Yeni dosyalar kopyalaniyor..." << std::endl;
        bat << "xcopy /E /Y /Q \"" << narrowSourceDir << "\\*\" \"" << narrowExeDir << "\\\" >nul 2>&1" << std::endl;
        bat << std::endl;
        bat << "REM Restart the application" << std::endl;
        bat << "echo Uygulama yeniden baslatiliyor..." << std::endl;
        bat << "start \"\" \"" << narrowExePath << "\"" << std::endl;
        bat << std::endl;
        bat << "REM Cleanup temp files" << std::endl;
        bat << "echo Gecici dosyalar temizleniyor..." << std::endl;
        bat << "timeout /t 2 /nobreak >nul" << std::endl;
        bat << "rmdir /S /Q \"" << narrowExtractedFolder << "\" >nul 2>&1" << std::endl;
        bat << "del /F /Q \"" << narrowZipPath << "\" >nul 2>&1" << std::endl;
        bat << std::endl;
        bat << "REM Delete self" << std::endl;
        bat << "del /F /Q \"" << narrowBatPath << "\" >nul 2>&1" << std::endl;
        bat.close();

        // Launch the batch script hidden
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};
        std::wstring cmdLine = L"cmd.exe /C \"" + batPath + L"\"";
        
        if (!CreateProcessW(NULL, &cmdLine[0], NULL, NULL, FALSE,
                            CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            outError = "Guncelleme script'i baslatilamadi.";
            return false;
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    // --------------------------------------------------------------------------
    // Public API
    // --------------------------------------------------------------------------

    void UpdateChecker::CheckForUpdateAsync(std::function<void(const UpdateInfo&)> callback) {
        std::thread([callback]() {
            UpdateInfo info = {false, "", "", "", ""};
            
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

                // Find the ZIP asset download URL
                std::string zipDownloadUrl;
                if (j.contains("assets") && j["assets"].is_array()) {
                    for (auto& asset : j["assets"]) {
                        std::string name = asset.value("name", "");
                        // Look for .zip files
                        if (name.size() > 4 && name.substr(name.size() - 4) == ".zip") {
                            zipDownloadUrl = asset.value("browser_download_url", "");
                            break;
                        }
                    }
                }

                int remoteMajor = 0, remoteMinor = 0, remotePatch = 0;
                if (ParseVersion(tag_name, remoteMajor, remoteMinor, remotePatch)) {
                    if (remoteMajor > APP_VERSION_MAJOR ||
                       (remoteMajor == APP_VERSION_MAJOR && remoteMinor > APP_VERSION_MINOR) ||
                       (remoteMajor == APP_VERSION_MAJOR && remoteMinor == APP_VERSION_MINOR && remotePatch > APP_VERSION_PATCH)) {
                        
                        info.hasUpdate = true;
                        info.version = tag_name;
                        info.releaseUrl = html_url;
                        info.downloadUrl = zipDownloadUrl;
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

    void UpdateChecker::DownloadAndApplyUpdateAsync(
        const std::string& downloadUrl,
        std::atomic<float>* progress,
        std::function<void(bool success, const std::string& errorMessage)> completionCallback) {

        std::thread([downloadUrl, progress, completionCallback]() {
            std::string error;

            // 1. Build temp paths
            wchar_t tempPath[MAX_PATH];
            GetTempPathW(MAX_PATH, tempPath);
            std::wstring zipPath = std::wstring(tempPath) + L"WallpaperAnim_update.zip";
            std::wstring extractDir = std::wstring(tempPath) + L"WallpaperAnim_update";

            // Clean up any previous update attempt
            DeleteFileW(zipPath.c_str());
            // Remove old extract dir recursively
            {
                SHFILEOPSTRUCTW fileOp = {};
                std::wstring doubleNull = extractDir + L'\0';
                fileOp.wFunc = FO_DELETE;
                fileOp.pFrom = doubleNull.c_str();
                fileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
                SHFileOperationW(&fileOp);
            }

            // 2. Download the ZIP
            if (!DownloadFileToPath(downloadUrl, zipPath, progress, error)) {
                completionCallback(false, error.empty() ? "Dosya indirilemedi." : error);
                return;
            }

            // 3. Extract the ZIP
            if (!ExtractZipToFolder(zipPath, extractDir, error)) {
                completionCallback(false, error.empty() ? "ZIP acilamadi." : error);
                return;
            }

            // 4. Create and launch the updater batch script
            if (!CreateAndLaunchUpdater(extractDir, error)) {
                completionCallback(false, error.empty() ? "Guncelleme script'i olusturulamadi." : error);
                return;
            }

            // 5. Exit the application — the batch script will handle the rest
            ExitProcess(0);

        }).detach();
    }
}
