#pragma once
#include <string>

namespace Utils {
    class ThumbnailGenerator {
    public:
        // Generates a thumbnail for a given video/gif file.
        // Saves it to AppData/Local/WallpaperAnim/Thumbnails/<hash>.jpg
        // Returns the path to the generated thumbnail, or empty string if failed.
        static std::wstring GenerateThumbnail(const std::wstring& mediaPath);
        
        static std::wstring GetThumbnailsDirectory();
    };
}
