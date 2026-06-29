#include "tray.h"
#include "resource.h"

namespace SystemTray {

    TrayIcon::TrayIcon() : m_hwnd(nullptr), m_isPaused(false) {
        ZeroMemory(&m_nid, sizeof(m_nid));
    }

    TrayIcon::~TrayIcon() {
        Cleanup();
    }

    bool TrayIcon::Initialize(HWND hwnd) {
        m_hwnd = hwnd;

        m_nid.cbSize = sizeof(NOTIFYICONDATA);
        m_nid.hWnd = hwnd;
        m_nid.uID = 1;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = WM_TRAYICON;
        // Use a custom icon from resources
        m_nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APP_ICON));
        lstrcpy(m_nid.szTip, L"WallpaperAnim");

        return Shell_NotifyIcon(NIM_ADD, &m_nid) == TRUE;
    }

    void TrayIcon::Cleanup() {
        if (m_hwnd) {
            Shell_NotifyIcon(NIM_DELETE, &m_nid);
            m_hwnd = nullptr;
        }
    }

    void TrayIcon::HandleTrayMessage(LPARAM lParam) {
        switch (LOWORD(lParam)) {
        case WM_RBUTTONUP:
            ShowContextMenu();
            break;
        case WM_LBUTTONDBLCLK:
            SendMessage(m_hwnd, WM_COMMAND, ID_TRAY_SETTINGS, 0);
            break;
        }
    }

    void TrayIcon::ShowContextMenu() {
        POINT pt;
        GetCursorPos(&pt);

        HMENU hMenu = CreatePopupMenu();
        if (m_isPaused) {
            AppendMenu(hMenu, MF_STRING, ID_TRAY_PLAY, L"Play");
        } else {
            AppendMenu(hMenu, MF_STRING, ID_TRAY_PAUSE, L"Pause");
        }
        AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenu(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"Settings...");
        AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

        // Required to prevent the menu from staying open if clicking elsewhere
        SetForegroundWindow(m_hwnd);

        UINT clicked = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_hwnd, nullptr);
        
        SendMessage(m_hwnd, WM_COMMAND, clicked, 0);

        DestroyMenu(hMenu);
    }
}
