#pragma once
#include <string>
#include <functional>
#include <atomic>

namespace Utils {
    struct UpdateInfo {
        bool hasUpdate;
        std::string version;
        std::string releaseUrl;
        std::string downloadUrl;  // Direct ZIP download URL
        std::string errorMessage;
    };

    class UpdateChecker {
    public:
        // Returns true if the check started successfully.
        // callback(UpdateInfo) will be called on a background thread.
        static void CheckForUpdateAsync(std::function<void(const UpdateInfo&)> callback);

        // Downloads the update ZIP, extracts it, creates a batch updater script,
        // launches it, and exits the application.
        // progressCallback is called with a value between 0.0 and 1.0.
        // completionCallback is called with (success, errorMessage).
        // On success, the application will exit before completionCallback is called.
        static void DownloadAndApplyUpdateAsync(
            const std::string& downloadUrl,
            std::atomic<float>* progress,
            std::function<void(bool success, const std::string& errorMessage)> completionCallback
        );
    };
}
