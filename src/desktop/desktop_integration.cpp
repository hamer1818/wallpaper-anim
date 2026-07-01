#include "desktop_integration.h"
#include <shlobj.h>
#include <fstream>
#include <string>

namespace DesktopIntegration {

    static HWND g_workerW = nullptr;

    void DiagLog(const std::string& msg) {
        PWSTR path = nullptr;
        std::wstring dir;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
            dir = path;
            dir += L"\\WallpaperAnim";
            CreateDirectoryW(dir.c_str(), nullptr);
            CoTaskMemFree(path);
        }
        std::wstring logPath = dir.empty() ? L"wallpaper.log" : (dir + L"\\wallpaper.log");
        std::ofstream ofs(logPath, std::ios::app);
        if (ofs.is_open()) {
            ofs << "[" << GetTickCount64() << "] " << msg << "\n";
        }
    }

    static BOOL CALLBACK EnumWindowsProc(HWND top, LPARAM) {
        // A window that hosts SHELLDLL_DefView is the shell view holder; the WorkerW that
        // follows it in z-order is the empty layer behind the icons where wallpapers go.
        HWND shell = FindWindowEx(top, nullptr, L"SHELLDLL_DefView", nullptr);
        if (shell != nullptr) {
            HWND w = FindWindowEx(nullptr, top, L"WorkerW", nullptr);
            if (w != nullptr) {
                g_workerW = w;
            }
        }
        return TRUE;
    }

    bool SetupWallpaperWindow(HWND hwnd) {
        g_workerW = nullptr;

        HWND progman = FindWindow(L"Progman", nullptr);
        DiagLog("SetupWallpaperWindow: Progman=" + std::to_string((uintptr_t)progman));
        if (!progman) {
            DiagLog("FAIL: Progman not found");
            return false;
        }

        // Ask Progman to spawn the WorkerW layer behind the desktop icons. WorkerW
        // creation can be asynchronous, so poll the enumeration a few times.
        DWORD_PTR result = 0;
        SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result);
        for (int i = 0; i < 10 && !g_workerW; ++i) {
            EnumWindows(EnumWindowsProc, 0);
            if (!g_workerW) Sleep(80);
        }
        DiagLog("After 0x052C(0,0): WorkerW=" + std::to_string((uintptr_t)g_workerW));

        // Some Windows 10 builds only create the icon-backing WorkerW with the (0xD, 0x1)
        // variant. Retry with it if the first attempt did not surface a WorkerW.
        if (!g_workerW) {
            SendMessageTimeout(progman, 0x052C, 0xD, 0x1, SMTO_NORMAL, 1000, &result);
            for (int i = 0; i < 10 && !g_workerW; ++i) {
                EnumWindows(EnumWindowsProc, 0);
                if (!g_workerW) Sleep(80);
            }
            DiagLog("After 0x052C(0xD,1): WorkerW=" + std::to_string((uintptr_t)g_workerW));
        }

        HWND target = g_workerW ? g_workerW : progman;
        DiagLog(g_workerW ? "Parenting to WorkerW" : "Parenting to Progman (fallback)");

        SetLastError(0);
        HWND prev = SetParent(hwnd, target);
        DiagLog("SetParent prev=" + std::to_string((uintptr_t)prev) +
                " lastError=" + std::to_string(GetLastError()));

        // Turn the popup into a child of the desktop layer.
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style &= ~WS_POPUP;
        style |= WS_CHILD;
        SetWindowLongPtr(hwnd, GWL_STYLE, style);

        int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        // Put it at the bottom of the parent's z-order (behind the icons) and show it.
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, screenWidth, screenHeight,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        ShowWindow(hwnd, SW_SHOWNA);

        RECT rc = {};
        GetWindowRect(hwnd, &rc);
        DiagLog("Positioned window rect=(" + std::to_string(rc.left) + "," + std::to_string(rc.top) +
                "," + std::to_string(rc.right) + "," + std::to_string(rc.bottom) + ") visible=" +
                std::to_string(IsWindowVisible(hwnd)));

        return true;
    }

    void RestoreWallpaperWindow(HWND hwnd) {
        SetParent(hwnd, nullptr);
    }
}
