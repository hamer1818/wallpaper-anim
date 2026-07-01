#include "desktop_integration.h"
#include <iostream>

namespace DesktopIntegration {

    HWND g_workerW = nullptr;

    BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
        HWND p = FindWindowEx(hwnd, nullptr, L"SHELLDLL_DefView", nullptr);
        if (p != nullptr) {
            // Gets the WorkerW Window after the current one.
            g_workerW = FindWindowEx(nullptr, hwnd, L"WorkerW", nullptr);
        }
        return TRUE;
    }

    bool SetupWallpaperWindow(HWND hwnd) {
        // Fetch the Progman window
        HWND progman = FindWindow(L"Progman", nullptr);
        if (!progman) return false;

        // Send 0x052C to Progman. This message directs Progman to spawn a 
        // WorkerW behind the desktop icons. If it is already there, nothing 
        // happens.
        SendMessageTimeout(progman,
            0x052C,
            0,
            0,
            SMTO_NORMAL,
            1000,
            nullptr);

        // We enumerate all Windows, until we find one, that has the SHELLDLL_DefView 
        // as a child. 
        // If we found that window, we take its next sibling and assign it to workerw.
        EnumWindows(EnumWindowsProc, 0);

        HWND targetParent = g_workerW;
        if (targetParent == nullptr) {
            OutputDebugString(L"WorkerW not found, falling back to Progman.\n");
            targetParent = progman;
        }

        if (targetParent != nullptr) {
            // Set the new parent
            SetParent(hwnd, targetParent);
            
            // Convert to child window to prevent it from acting as a fullscreen overlay if something goes wrong
            LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
            style &= ~WS_POPUP;
            style |= WS_CHILD;
            SetWindowLongPtr(hwnd, GWL_STYLE, style);

            // Cover the whole virtual desktop (all monitors), not just the primary screen.
            int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

            // Attempt to put it behind the icons
            HWND defView = FindWindowEx(progman, nullptr, L"SHELLDLL_DefView", nullptr);
            if (defView) {
                // Insert after defView (which means below it in Z-order)
                SetWindowPos(hwnd, defView, 0, 0, screenWidth, screenHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            } else {
                SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, screenWidth, screenHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            }
            return true;
        }

        return false;
    }

    void RestoreWallpaperWindow(HWND hwnd) {
        SetParent(hwnd, nullptr);
    }
}
