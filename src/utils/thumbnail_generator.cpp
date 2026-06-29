#include "thumbnail_generator.h"
#include <windows.h>
#include <shlobj.h>
#include <functional>

namespace Utils {

    std::wstring ThumbnailGenerator::GetThumbnailsDirectory() {
        PWSTR path = NULL;
        std::wstring dir;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path))) {
            dir = path;
            dir += L"\\WallpaperAnim";
            CreateDirectoryW(dir.c_str(), NULL);
            dir += L"\\Thumbnails";
            CreateDirectoryW(dir.c_str(), NULL);
            CoTaskMemFree(path);
        }
        return dir;
    }

    std::wstring ThumbnailGenerator::GenerateThumbnail(const std::wstring& mediaPath) {
        if (mediaPath.empty()) return L"";

        // If it's a shader, return a placeholder (assume assets/shader_thumb.png exists)
        if (mediaPath.find(L".hlsl") != std::wstring::npos) {
            return L"assets\\shader_thumb.png"; // We can create a basic image later
        }

        std::wstring thumbDir = GetThumbnailsDirectory();
        if (thumbDir.empty()) return L"";

        // Generate a hash based on the file path to use as the thumbnail filename
        std::hash<std::wstring> hasher;
        size_t hash = hasher(mediaPath);
        std::wstring thumbPath = thumbDir + L"\\" + std::to_wstring(hash) + L".jpg";

        // Check if it already exists
        DWORD attrib = GetFileAttributesW(thumbPath.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            return thumbPath;
        }

        // Run ffmpeg to extract a frame
        // ffmpeg -y -i "mediaPath" -ss 00:00:00.000 -vframes 1 "thumbPath"
        std::wstring cmd = L"ffmpeg -y -i \"" + mediaPath + L"\" -ss 00:00:00.000 -vframes 1 \"" + thumbPath + L"\"";
        
        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = { 0 };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE; // Hide console window

        if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 10000); // Wait up to 10 seconds
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            
            // Check if successful
            attrib = GetFileAttributesW(thumbPath.c_str());
            if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                return thumbPath;
            }
        }

        return L"";
    }
}
