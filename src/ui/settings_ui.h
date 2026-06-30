#pragma once

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

#include "utils/youtube_downloader.h"
#include "utils/update_checker.h"
#include <map>
#include <atomic>
#include <vector>

class SettingsUI {
public:
    SettingsUI();
    ~SettingsUI();

    bool Initialize(HINSTANCE hInstance, ID3D11Device* device, HWND mainHwnd);
    void Show();
    void Hide();
    bool IsVisible() const { return m_isVisible; }
    void Render();

    HWND GetHWND() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void RenderUI();
    void Cleanup();
    bool CreateDeviceAndSwapChain();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    std::wstring OpenFileDialog();

    HINSTANCE m_hInstance;
    HWND m_hwnd;
    HWND m_mainHwnd;
    bool m_isVisible;
    bool m_imguiInitialized;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_mainRenderTargetView;

    // YouTube State
    char m_ytUrl[512] = "";
    bool m_ytFetching = false;
    bool m_ytDownloading = false;
    std::atomic<float> m_ytProgress{0.0f};
    std::string m_ytTitle;
    std::vector<Utils::YouTubeResolution> m_ytResolutions;
    int m_ytSelectedRes = 0;

    // Async Callback Queue state
    bool m_ytFetchComplete = false;
    bool m_ytFetchSuccess = false;
    std::vector<Utils::YouTubeResolution> m_ytFetchResResult;
    std::string m_ytFetchTitleResult;
    std::string m_ytFetchErrResult;

    bool m_ytDownloadComplete = false;
    bool m_ytDownloadSuccess = false;
    std::wstring m_ytDownloadPathResult;
    std::string m_ytDownloadErrResult;

    // Error State
    std::string m_lastErrorMsg = "";
    bool m_showError = false;

    // Update State
    bool m_updateChecking = false;
    bool m_showUpdatePopup = false;
    Utils::UpdateInfo m_updateInfo;
    void CheckForUpdate(bool showPopupIfUpToDate);
    bool m_showUpdateUpToDate = false;

    // Auto-Update Download State
    bool m_updateDownloading = false;
    std::atomic<float> m_updateProgress{0.0f};
    bool m_updateDownloadComplete = false;
    bool m_updateDownloadSuccess = false;
    std::string m_updateDownloadError;

    // Thumbnail Cache
    std::map<std::wstring, ComPtr<ID3D11ShaderResourceView>> m_thumbnails;
    ID3D11ShaderResourceView* GetThumbnailSRV(const std::wstring& thumbPath);

    ComPtr<ID3D11ShaderResourceView> m_logoSrv;
    int m_logoWidth = 0;
    int m_logoHeight = 0;

    void AddToHistory(const std::wstring& path, int type);
    void PlayMedia(const std::wstring& path);
};
