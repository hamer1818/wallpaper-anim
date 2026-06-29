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

        // Starts an async download
        // Uses atomic float to report progress (0.0 to 1.0)
        // Callback returns true on success, with the final file path
        static void DownloadAsync(const std::string& url, const std::string& format_id, std::atomic<float>* progress, std::function<void(bool, const std::wstring&, const std::string& error_msg)> callback);

    private:
        static std::wstring GetCacheDirectory();
    };
}
