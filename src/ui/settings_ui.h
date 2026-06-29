#pragma once

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

#include "utils/youtube_downloader.h"
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
    std::atomic<float> m_ytProgress = 0.0f;
    std::vector<Utils::YouTubeResolution> m_ytResolutions;
    int m_ytSelectedRes = 0;
    std::string m_ytTitle = "";

    // Error State
    std::string m_lastErrorMsg = "";
    bool m_showError = false;

    // Thumbnail Cache
    std::map<std::wstring, ComPtr<ID3D11ShaderResourceView>> m_thumbnails;
    ID3D11ShaderResourceView* GetThumbnailSRV(const std::wstring& thumbPath);

    void AddToHistory(const std::wstring& path, int type);
    void PlayMedia(const std::wstring& path);
};
