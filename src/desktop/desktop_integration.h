#pragma once

#include <windows.h>
#include <string>

namespace DesktopIntegration {
    // Finds the appropriate WorkerW window and sets the provided window as its child.
    // This allows the window to render behind desktop icons.
    bool SetupWallpaperWindow(HWND hwnd);

    // Reverts the wallpaper window to a normal window (detaches from WorkerW).
    void RestoreWallpaperWindow(HWND hwnd);

    // Appends a line to %LocalAppData%\WallpaperAnim\wallpaper.log. Always on (even in
    // Release) so users can send us this file to diagnose desktop-integration issues.
    void DiagLog(const std::string& msg);
}
