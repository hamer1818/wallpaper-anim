#pragma once
#include "App.g.h"
#include <windows.h>
#include <thread>
#include <atomic>
#include <string>
#include "src/render/dx11_renderer.h"
#include "src/tray.h"

#define WM_APP_CONFIG_CHANGED (WM_APP + 1)

namespace winrt::WallpaperAnimWinUI::implementation
{
    struct App : AppT<App>
    {
        App();
        ~App();
        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);
        
        HWND GetWallpaperHwnd() const { return m_wallpaperHwnd; }
        static App* GetInstance() { return s_instance; }

    private:
        static App* s_instance;
        winrt::Microsoft::UI::Xaml::Window window{ nullptr };
        
        HWND m_wallpaperHwnd{ nullptr };
        Render::DX11Renderer m_renderer;
        SystemTray::TrayIcon m_trayIcon;
        std::thread m_renderThread;
        std::atomic<bool> m_running{ false };
        std::atomic<bool> m_isPaused{ false };

        void LoadMedia(const std::wstring& path);
        void RenderLoop();
        static LRESULT CALLBACK WallpaperWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleWallpaperMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    };
}
namespace winrt::WallpaperAnimWinUI::factory_implementation
{
    struct App : AppT<App, implementation::App>
    {
    };
}
