#pragma once
#include <string>
#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace Utils {
    class ThumbnailGenerator {
    public:
        // Generates a thumbnail for a given video/gif file.
        // Uses Media Foundation to extract a frame at ~2 seconds (no ffmpeg needed).
        // Saves it to AppData/Local/WallpaperAnim/Thumbnails/<hash>.jpg
        // Returns the path to the generated thumbnail, or empty string if failed.
        static std::wstring GenerateThumbnail(const std::wstring& mediaPath);
        
        static std::wstring GetThumbnailsDirectory();

        // Force regenerate thumbnail (delete existing and create new)
        static std::wstring RegenerateThumbnail(const std::wstring& mediaPath);
    };
}
