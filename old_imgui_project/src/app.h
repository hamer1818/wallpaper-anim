#pragma once
#include <windows.h>
#include "render/dx11_renderer.h"
#include "tray.h"
#include "ui/settings_ui.h"

#define WM_APP_CONFIG_CHANGED (WM_APP + 1)

class App {
public:
    App();
    ~App();

    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void LoadMedia(const std::wstring& path);

    HWND m_hwnd;
    HINSTANCE m_hInstance;
    Render::DX11Renderer m_renderer;
    SystemTray::TrayIcon m_trayIcon;
    SettingsUI m_settingsUI;
    bool m_isPaused = false;
};
