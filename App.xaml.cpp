#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <fstream>
#include <string>

void LogApp(const std::string& msg) {
#ifdef _DEBUG
    std::ofstream ofs("debug2.log", std::ios::app);
    if(ofs.is_open()) ofs << msg << std::endl;
#else
    (void)msg;
#endif
}
#include "src/desktop/desktop_integration.h"
#include "src/render/video_player.h"
#include "src/render/gif_player.h"
#include "src/render/shader_player.h"
#include "src/system/system_monitor.h"
#include "src/config.h"
#include "src/localization.h"
#include "src/resource.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Navigation;

namespace winrt::WallpaperAnimWinUI::implementation
{
    App* App::s_instance = nullptr;

    App::App()
    {
        s_instance = this;
        InitializeComponent();

        this->UnhandledException([](winrt::Windows::Foundation::IInspectable const&, winrt::Microsoft::UI::Xaml::UnhandledExceptionEventArgs const& e)
        {
            e.Handled(true);
            std::wstring msg = L"Unhandled Exception: " + std::wstring(e.Message());
            MessageBoxW(nullptr, msg.c_str(), L"Fatal Error", MB_OK | MB_ICONERROR);
            ExitProcess(1);
        });
    }

    App::~App()
    {
        s_instance = nullptr;
        m_running = false;
        if (m_renderThread.joinable()) {
            m_renderThread.join();
        }
        m_trayIcon.Cleanup();
        if (m_wallpaperHwnd) {
            DesktopIntegration::RestoreWallpaperWindow(m_wallpaperHwnd);
            DestroyWindow(m_wallpaperHwnd);
        }
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        LogApp("Entered App::OnLaunched");
        // Load config
        Config::ConfigManager::GetInstance().Load();
        auto& config = Config::ConfigManager::GetInstance().GetConfig();

        // Create the Wallpaper HWND
        HINSTANCE hInstance = GetModuleHandle(nullptr);
        const wchar_t CLASS_NAME[] = L"WallpaperAnimClassWinUI";

        WNDCLASS wc = { };
        wc.lpfnWndProc   = App::WallpaperWindowProc;
        wc.hInstance     = hInstance;
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);

        RegisterClass(&wc);

        // Span the entire virtual desktop so the wallpaper can cover every monitor.
        int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int vcx = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int vcy = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        m_wallpaperHwnd = CreateWindowEx(
            0,
            CLASS_NAME,
            L"WallpaperAnimBackground",
            WS_POPUP | WS_VISIBLE,
            vx, vy, vcx, vcy,
            nullptr, nullptr, hInstance, this
        );

        DesktopIntegration::DiagLog("OnLaunched: wallpaperHwnd=" + std::to_string((uintptr_t)m_wallpaperHwnd) +
                                    " virt=(" + std::to_string(vx) + "," + std::to_string(vy) + "," +
                                    std::to_string(vcx) + "," + std::to_string(vcy) + ")");
        if (m_wallpaperHwnd == nullptr) {
            DesktopIntegration::DiagLog("FAIL: CreateWindowEx returned null, err=" + std::to_string(GetLastError()));
            return;
        }

        // Initialize DirectX 11 on the wallpaper window
        LogApp("Initializing Renderer...");
        if (!m_renderer.Initialize(m_wallpaperHwnd)) {
            DesktopIntegration::DiagLog("FAIL: DX11 renderer init failed");
            MessageBox(nullptr, L"DirectX 11 baslatilamadi.", L"Hata", MB_OK | MB_ICONERROR);
            return;
        }
        DesktopIntegration::DiagLog("Renderer initialized OK");

        // Setup Desktop Integration (WorkerW parenting)
        LogApp("Setting up Desktop Integration...");
        if (!DesktopIntegration::SetupWallpaperWindow(m_wallpaperHwnd)) {
            DesktopIntegration::DiagLog("FAIL: SetupWallpaperWindow returned false");
            MessageBox(nullptr, L"Masaustu entegrasyonu basarisiz oldu.", L"Hata", MB_OK | MB_ICONERROR);
            return;
        }

        // Initialize Tray Icon
        LogApp("Initializing Tray Icon...");
        m_trayIcon.Initialize(m_wallpaperHwnd);

        // Load initial media
        std::wstring videoPath = config.lastVideoPath;
        if (videoPath.empty() || videoPath == L"assets\\sample.mp4") {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(nullptr, path, MAX_PATH);
            std::wstring exePath = path;
            videoPath = exePath.substr(0, exePath.find_last_of(L"\\/")) + L"\\assets\\sample.mp4";
        }
        bool initialOk = LoadMedia(videoPath);
        DesktopIntegration::DiagLog("Initial LoadMedia ok=" + std::to_string(initialOk));

        // Start background rendering thread
        m_running = true;
        m_renderThread = std::thread(&App::RenderLoop, this);

        LogApp("ShowWindow called");

        // Boot WinUI UI Window
        LogApp("Creating MainWindow...");
        try {
            window = make<MainWindow>();
            LogApp("Activating MainWindow...");
            window.Activate();
            LogApp("MainWindow Activated");
        } catch (winrt::hresult_error const& ex) {
            std::wstring msg = L"MainWindow başlatılamadı:\n" + std::wstring(ex.message());
            MessageBoxW(nullptr, msg.c_str(), L"Arayüz Hatası", MB_OK | MB_ICONERROR);
        } catch (std::exception const& ex) {
            std::wstring msg = L"MainWindow başlatılamadı (std::exception):\n";
            std::string s(ex.what());
            msg += std::wstring(s.begin(), s.end());
            MessageBoxW(nullptr, msg.c_str(), L"Arayüz Hatası", MB_OK | MB_ICONERROR);
        } catch (...) {
            MessageBoxW(nullptr, L"MainWindow başlatılırken bilinmeyen bir hata oluştu.", L"Arayüz Hatası", MB_OK | MB_ICONERROR);
        }
    }

    bool App::LoadMedia(const std::wstring& path)
    {
        std::wstring ext = path.substr(path.find_last_of(L".") + 1);
        for (auto& c : ext) c = towlower(c);

        std::unique_ptr<Render::IMediaPlayer> newPlayer;
        if (ext == L"gif") {
            newPlayer = std::make_unique<Render::GifPlayer>();
        } else if (ext == L"hlsl") {
            newPlayer = std::make_unique<Render::ShaderPlayer>();
        } else {
            newPlayer = std::make_unique<Render::VideoPlayer>();
        }

        // Prepare the new player before handing it to the renderer so the swap (which
        // stops/cleans the old player under the render lock) is as quick as possible.
        bool ok = false;
        if (newPlayer->Initialize(m_renderer.GetDevice(), m_renderer.GetContext())) {
            if (newPlayer->LoadMedia(path)) {
                newPlayer->Play();
                ok = true;
            }
        }

        if (ok) {
            m_renderer.SetMediaPlayer(std::move(newPlayer));
        } else {
            // Loading failed (e.g. a codec Media Foundation can't decode on this OS).
            // Discard the new player and keep whatever was already playing.
            newPlayer->Cleanup();
        }
        return ok;
    }

    // Current primary-monitor refresh rate (Hz), falling back to 60 if unknown.
    static int GetPrimaryRefreshRate()
    {
        DEVMODEW dm = {};
        dm.dmSize = sizeof(dm);
        if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1) {
            return (int)dm.dmDisplayFrequency;
        }
        return 60;
    }

    // Maps the configured target FPS onto a DXGI present sync interval (1..4). The
    // hardware then paces frames on the vblank, which is smooth/judder-free. maxFPS <= 0
    // means "match the monitor" (sync interval 1 = full refresh rate).
    static UINT ComputeSyncInterval(int maxFPS, int refreshRate)
    {
        if (maxFPS <= 0 || maxFPS >= refreshRate) return 1;
        int iv = (int)((double)refreshRate / maxFPS + 0.5);
        if (iv < 1) iv = 1;
        if (iv > 4) iv = 4;
        return (UINT)iv;
    }

    void App::RenderLoop()
    {
        LARGE_INTEGER freq, lastCheckTime;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&lastCheckTime);

        bool isAutoPaused = false;

        int refreshRate = GetPrimaryRefreshRate();
        UINT syncInterval = ComputeSyncInterval(Config::ConfigManager::GetInstance().GetConfig().maxFPS, refreshRate);

        while (m_running) {
            LARGE_INTEGER frameStart;
            QueryPerformanceCounter(&frameStart);
            double elapsedCheck = (double)(frameStart.QuadPart - lastCheckTime.QuadPart) / freq.QuadPart;

            // Check system state (battery / fullscreen) roughly once per second.
            if (elapsedCheck >= 1.0) {
                lastCheckTime = frameStart;

                auto& config = Config::ConfigManager::GetInstance().GetConfig();
                bool shouldAutoPause = false;

                if (config.pauseOnBattery && SystemMonitor::IsOnBattery()) {
                    shouldAutoPause = true;
                }
                if (config.pauseOnFullscreen && SystemMonitor::IsFullscreenAppActive()) {
                    shouldAutoPause = true;
                }

                isAutoPaused = shouldAutoPause;

                // Refresh rate can change (e.g. monitor switch); recompute pacing.
                refreshRate = GetPrimaryRefreshRate();
                syncInterval = ComputeSyncInterval(config.maxFPS, refreshRate);
            }

            // NOTE: We intentionally do NOT pause on DXGI_STATUS_OCCLUDED (some Win10
            // WorkerW-parented windows report themselves permanently occluded, which would
            // stop the wallpaper forever). Resource use is bounded by the sync interval and
            // the fullscreen/battery auto-pause above.
            if (!m_isPaused && !isAutoPaused) {
                m_renderer.RenderFrame();
                // Hardware VSync pacing — no Sleep-based limiter, so no judder even on
                // high-refresh (144 Hz+) monitors.
                m_renderer.Present(syncInterval);
            } else {
                Sleep(100); // manual/auto pause: idle cheaply
            }
        }
    }

    LRESULT CALLBACK App::WallpaperWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        App* pThis = nullptr;
        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            pThis = (App*)pCreate->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
            pThis->m_wallpaperHwnd = hwnd;
        } else {
            pThis = (App*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }

        if (pThis) {
            return pThis->HandleWallpaperMessage(hwnd, uMsg, wParam, lParam);
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    LRESULT App::HandleWallpaperMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg) {
        case WM_DESTROY:
            m_running = false;
            PostQuitMessage(0);
            return 0;

        case WM_APP_CONFIG_CHANGED: {
            auto& config = Config::ConfigManager::GetInstance().GetConfig();
            std::wstring p = config.lastVideoPath;
            DesktopIntegration::DiagLog("WM_APP_CONFIG_CHANGED: loading new media (len=" + std::to_string(p.size()) + ")");
            if (!LoadMedia(config.lastVideoPath)) {
                DesktopIntegration::DiagLog("WM_APP_CONFIG_CHANGED: LoadMedia FAILED");
                // Tell the user why nothing changed instead of failing silently.
                auto& strings = Localization::Get();
                auto toWide = [](const char* utf8) {
                    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
                    std::wstring w(len > 0 ? len - 1 : 0, L'\0');
                    if (len > 0) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], len);
                    return w;
                };
                MessageBoxW(nullptr, toWide(strings.mediaLoadFailed).c_str(),
                            toWide(strings.mediaLoadFailedTitle).c_str(), MB_OK | MB_ICONWARNING);
            }
            return 0;
        }

        case WM_DISPLAYCHANGE: {
            // Resolution / monitor layout changed: resize the window and swap chain to
            // the new virtual-desktop size so the wallpaper keeps filling every monitor.
            int vcx = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            int vcy = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            SetWindowPos(hwnd, nullptr, 0, 0, vcx, vcy, SWP_NOZORDER | SWP_NOACTIVATE);
            m_renderer.Resize((UINT)vcx, (UINT)vcy);
            return 0;
        }

        case SystemTray::WM_TRAYICON:
            m_trayIcon.HandleTrayMessage(lParam);
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
            case SystemTray::ID_TRAY_SETTINGS:
                if (window) {
                    window.AppWindow().Show();
                    window.Activate();
                }
                break;
            case SystemTray::ID_TRAY_EXIT:
                m_running = false;
                ExitProcess(0);
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
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}
