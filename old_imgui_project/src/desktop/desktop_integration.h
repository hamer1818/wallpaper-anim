#pragma once

#include <windows.h>

namespace DesktopIntegration {
    // Finds the appropriate WorkerW window and sets the provided window as its child.
    // This allows the window to render behind desktop icons.
    bool SetupWallpaperWindow(HWND hwnd);

    // Reverts the wallpaper window to a normal window (detaches from WorkerW).
    void RestoreWallpaperWindow(HWND hwnd);
}
