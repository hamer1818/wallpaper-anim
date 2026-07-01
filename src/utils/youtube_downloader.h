#pragma once
#include <string>
#include <vector>
#include <functional>
#include <atomic>

namespace Utils {
    
    struct YouTubeResolution {
        std::string format_id;
        int height;
        std::string ext;
    };

    class YouTubeDownloader {
    public:
        // Starts an async fetch for available qualities
        // Callback returns true on success, with a list of resolutions
        static void FetchResolutionsAsync(const std::string& url, std::function<void(bool, const std::vector<YouTubeResolution>&, const std::string& title, const std::string& error_msg)> callback);

        // Starts an async download.
        // Picks an H.264 (avc1) stream no taller than maxHeight so the result plays on
        // every Windows version (Media Foundation can't decode VP9/AV1 without extra
        // codecs, which Windows 10 usually lacks). Pass 0 for maxHeight to auto-detect
        // from the primary monitor's height.
        // Uses atomic float to report progress (0.0 to 1.0).
        // Callback returns true on success, with the final file path.
        static void DownloadAsync(const std::string& url, int maxHeight, std::atomic<float>* progress, std::function<void(bool, const std::wstring&, const std::string& error_msg)> callback);

    private:
        static std::wstring GetCacheDirectory();
    };
}
