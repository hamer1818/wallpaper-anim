#pragma once
#include <string>
#include <functional>

namespace Utils {
    struct UpdateInfo {
        bool hasUpdate;
        std::string version;
        std::string releaseUrl;
        std::string errorMessage;
    };

    class UpdateChecker {
    public:
        // Returns true if the check started successfully.
        // callback(UpdateInfo) will be called on a background thread.
        static void CheckForUpdateAsync(std::function<void(const UpdateInfo&)> callback);
    };
}
