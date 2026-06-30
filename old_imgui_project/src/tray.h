#pragma once

#include <windows.h>
#include <shellapi.h>

namespace SystemTray {

    constexpr UINT WM_TRAYICON = WM_USER + 1;
    constexpr UINT ID_TRAY_EXIT = 1001;
    constexpr UINT ID_TRAY_PAUSE = 1002;
    constexpr UINT ID_TRAY_PLAY = 1003;
    constexpr UINT ID_TRAY_SETTINGS = 1004;

    class TrayIcon {
    public:
        TrayIcon();
        ~TrayIcon();

        bool Initialize(HWND hwnd);
        void Cleanup();

        void HandleTrayMessage(LPARAM lParam);
        void ShowContextMenu();

    private:
        HWND m_hwnd;
        NOTIFYICONDATA m_nid;
        bool m_isPaused;
    };
}
