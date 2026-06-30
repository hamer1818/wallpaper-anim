#include "app.h"
#include "desktop/desktop_integration.h"
#include "render/video_player.h"
#include "render/gif_player.h"
#include "render/shader_player.h"
#include "system/system_monitor.h"
#include "config.h"
#include "resource.h"
#include <iostream>

App::App() : m_hwnd(nullptr), m_hInstance(nullptr) {
}

App::~App() {
}

bool App::Initialize(HINSTANCE hInstance, int nCmdShow) {
    m_hInstance = hInstance;

    const wchar_t CLASS_NAME[] = L"WallpaperAnimClass";

    WNDCLASS wc = { };
    wc.lpfnWndProc   = App::WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&wc);

    // Create a frameless window
    m_hwnd = CreateWindowEx(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"WallpaperAnim",               // Window text
        WS_POPUP | WS_VISIBLE,          // Window style - frameless

        // Size and position
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),

        nullptr,       // Parent window    
        nullptr,       // Menu
        hInstance,     // Instance handle
        this           // Additional application data
    );

    if (m_hwnd == nullptr) {
        return false;
    }

    // Initialize DirectX 11
    if (!m_renderer.Initialize(m_hwnd)) {
        MessageBox(nullptr, L"DirectX 11 baslatilamadi.", L"Hata", MB_OK | MB_ICONERROR);
        return false;
    }

    // Initialize Settings UI
    if (!m_settingsUI.Initialize(hInstance, m_renderer.GetDevice(), m_hwnd)) {
        MessageBox(nullptr, L"Ayarlar arayuzu baslatilamadi.", L"Hata", MB_OK | MB_ICONERROR);
        return false;
    }

    Config::ConfigManager::GetInstance().Load();
    auto& config = Config::ConfigManager::GetInstance().GetConfig();
    std::wstring videoPath = config.lastVideoPath;
    if (videoPath.empty() || videoPath == L"assets\\sample.mp4") {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring exePath = path;
        videoPath = exePath.substr(0, exePath.find_last_of(L"\\/")) + L"\\assets\\sample.mp4";
    }

    LoadMedia(videoPath);

    if (config.isFirstRun) {
        m_settingsUI.Show();
    }

    // Set the window to draw behind desktop icons
    if (!DesktopIntegration::SetupWallpaperWindow(m_hwnd)) {
        MessageBox(nullptr, L"Masaustu entegrasyonu basarisiz oldu. Uygulama guvenli sekilde kapatiliyor.", L"Hata", MB_OK | MB_ICONERROR);
        return false;
    }

    // Initialize System Tray
    m_trayIcon.Initialize(m_hwnd);

    ShowWindow(m_hwnd, nCmdShow);

    return true;
}

int App::Run() {
    MSG msg = { };
    
    LARGE_INTEGER freq, lastCheckTime;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&lastCheckTime);
    
    bool isAutoPaused = false;

    while (true) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            LARGE_INTEGER currentTime;
            QueryPerformanceCounter(&currentTime);
            double elapsedCheck = (double)(currentTime.QuadPart - lastCheckTime.QuadPart) / freq.QuadPart;
            
            // Check system state every 1 second
            if (elapsedCheck >= 1.0) {
                lastCheckTime = currentTime;
                
                auto& config = Config::ConfigManager::GetInstance().GetConfig();
                bool shouldAutoPause = false;
                
                if (config.pauseOnBattery && SystemMonitor::IsOnBattery()) {
                    shouldAutoPause = true;
                }
                if (config.pauseOnFullscreen && SystemMonitor::IsFullscreenAppActive()) {
                    shouldAutoPause = true;
                }
                
                isAutoPaused = shouldAutoPause;
            }

            if (!m_isPaused && !isAutoPaused) {
                m_renderer.RenderFrame();
                m_renderer.Present();
            } else if (!m_settingsUI.IsVisible()) {
                // Sleep slightly to save CPU when paused, but don't sleep if Settings are visible
                Sleep(32);
            }

            if (m_settingsUI.IsVisible()) {
                m_settingsUI.Render();
            }
        }
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK App::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    App* pThis = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (App*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (App*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, uMsg, wParam, lParam);
    } else {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT App::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        Config::ConfigManager::GetInstance().Save();
        m_trayIcon.Cleanup();
        DesktopIntegration::RestoreWallpaperWindow(hwnd);
        PostQuitMessage(0);
        return 0;

    case WM_APP_CONFIG_CHANGED: {
        auto& config = Config::ConfigManager::GetInstance().GetConfig();
        LoadMedia(config.lastVideoPath);
        return 0;
    }

    case SystemTray::WM_TRAYICON:
        m_trayIcon.HandleTrayMessage(lParam);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case SystemTray::ID_TRAY_SETTINGS:
            m_settingsUI.Show();
            break;
        case SystemTray::ID_TRAY_EXIT:
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;
        case SystemTray::ID_TRAY_PAUSE:
            m_isPaused = true;
            if (m_renderer.GetMediaPlayer()) m_renderer.GetMediaPlayer()->Pause();
            break;
        case SystemTray::ID_TRAY_PLAY:
            m_isPaused = false;
            if (m_renderer.GetMediaPlayer()) m_renderer.GetMediaPlayer()->Play();
            break;
        }
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void App::LoadMedia(const std::wstring& path) {
    auto currentMediaPlayer = m_renderer.GetMediaPlayer();
    if (currentMediaPlayer) {
        currentMediaPlayer->Stop();
        currentMediaPlayer->Cleanup();
        delete currentMediaPlayer;
    }

    Render::IMediaPlayer* newPlayer = nullptr;

    // Check extension
    std::wstring ext = path.substr(path.find_last_of(L".") + 1);
    for (auto& c : ext) c = towlower(c);

    if (ext == L"gif") {
        newPlayer = new Render::GifPlayer();
    } else if (ext == L"hlsl") {
        newPlayer = new Render::ShaderPlayer();
    } else {
        newPlayer = new Render::VideoPlayer();
    }

    m_renderer.SetMediaPlayer(newPlayer);
    if (newPlayer->Initialize(m_renderer.GetDevice(), m_renderer.GetContext())) {
        newPlayer->LoadMedia(path);
        newPlayer->Play();
    }
}
