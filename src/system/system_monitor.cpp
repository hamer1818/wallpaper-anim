#include "system_monitor.h"

namespace SystemMonitor {

    bool IsOnBattery() {
        SYSTEM_POWER_STATUS status;
        if (GetSystemPowerStatus(&status)) {
            // ACLineStatus: 0 = Offline (Battery), 1 = Online (AC Power), 255 = Unknown status
            return (status.ACLineStatus == 0);
        }
        return false;
    }

    bool IsFullscreenAppActive() {
        HWND foregroundWin = GetForegroundWindow();
        if (!foregroundWin) return false;

        // Ignore desktop window and shell
        HWND desktopWin = GetDesktopWindow();
        HWND shellWin = GetShellWindow();
        if (foregroundWin == desktopWin || foregroundWin == shellWin) {
            return false;
        }

        char className[256];
        if (GetClassNameA(foregroundWin, className, sizeof(className))) {
            if (strcmp(className, "WorkerW") == 0 || strcmp(className, "Progman") == 0) {
                return false; // Clicking desktop icons creates a WorkerW or Progman foreground window
            }
        }

        // Get foreground window bounds
        RECT rc;
        if (!GetWindowRect(foregroundWin, &rc)) return false;

        // Get monitor the window is primarily on
        HMONITOR hMonitor = MonitorFromWindow(foregroundWin, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = { sizeof(mi) };
        if (!GetMonitorInfo(hMonitor, &mi)) return false;

        // Compare window size to monitor size
        int winWidth = rc.right - rc.left;
        int winHeight = rc.bottom - rc.top;
        int monWidth = mi.rcMonitor.right - mi.rcMonitor.left;
        int monHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

        // Allow a small margin of error (e.g., 2 pixels) for some borderless windowed games
        if (abs(winWidth - monWidth) <= 2 && abs(winHeight - monHeight) <= 2) {
            
            // Check if it's actually maximized/fullscreen, and not just a very large window
            // If the window rect completely covers the monitor rect:
            if (rc.left <= mi.rcMonitor.left && rc.top <= mi.rcMonitor.top &&
                rc.right >= mi.rcMonitor.right && rc.bottom >= mi.rcMonitor.bottom) {
                
                // One final check: some hidden windows or system overlays might cover the screen.
                // Ensure it's a visible, typical application window.
                DWORD style = GetWindowLong(foregroundWin, GWL_STYLE);
                if ((style & WS_VISIBLE) != 0) {
                    return true;
                }
            }
        }

        return false;
    }

    bool IsDesktopCovered() {
        // Only meaningful with a single monitor: with more than one screen, a maximized
        // window on one still leaves the wallpaper visible on the others, so pausing the
        // (single, shared) wallpaper would freeze those visible screens.
        if (GetSystemMetrics(SM_CMONITORS) != 1) return false;

        HWND foregroundWin = GetForegroundWindow();
        if (!foregroundWin) return false;

        // Ignore the desktop/shell (and the WorkerW/Progman windows that briefly take
        // foreground when clicking desktop icons) — those don't hide the wallpaper.
        HWND desktopWin = GetDesktopWindow();
        HWND shellWin = GetShellWindow();
        if (foregroundWin == desktopWin || foregroundWin == shellWin) return false;

        char className[256];
        if (GetClassNameA(foregroundWin, className, sizeof(className))) {
            if (strcmp(className, "WorkerW") == 0 || strcmp(className, "Progman") == 0) {
                return false;
            }
        }

        // A maximized, visible foreground window covers the whole work area; the only
        // still-visible strip is the (opaque) taskbar — never the wallpaper. That's
        // exactly when decoding the wallpaper is wasted work.
        DWORD style = GetWindowLong(foregroundWin, GWL_STYLE);
        if ((style & WS_VISIBLE) == 0) return false;
        if (style & WS_MAXIMIZE) return true;

        // Borderless windows that aren't flagged maximized but still cover the whole
        // monitor (e.g. some fullscreen media/games) — handled by IsFullscreenAppActive,
        // but catch them here too for completeness.
        RECT rc;
        if (!GetWindowRect(foregroundWin, &rc)) return false;
        HMONITOR hMonitor = MonitorFromWindow(foregroundWin, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = { sizeof(mi) };
        if (!GetMonitorInfo(hMonitor, &mi)) return false;
        if (rc.left <= mi.rcMonitor.left && rc.top <= mi.rcMonitor.top &&
            rc.right >= mi.rcMonitor.right && rc.bottom >= mi.rcMonitor.bottom) {
            return true;
        }

        return false;
    }
}
