#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <mutex>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace Render {
    class IMediaPlayer;

    class DX11Renderer {
    public:
        DX11Renderer();
        ~DX11Renderer();

        bool Initialize(HWND hwnd);
        void Cleanup();

        void RenderFrame();
        // Presents the current frame. syncInterval is the number of vblanks to wait
        // (1 = every refresh = smoothest at monitor Hz; 2 = half rate; etc.). Pacing is
        // done by the hardware here, which is judder-free unlike a Sleep-based limiter.
        HRESULT Present(UINT syncInterval = 1);
        // Non-rendering present used to poll whether the window is still occluded.
        HRESULT TestOcclusion();
        // Resizes the swap chain buffers (e.g. after a resolution / display change).
        bool Resize(UINT width, UINT height);

        ID3D11Device* GetDevice() const { return m_device.Get(); }
        ID3D11DeviceContext* GetContext() const { return m_context.Get(); }

        // Swaps in a new media player. The previous player is stopped, cleaned up and
        // destroyed under the render lock so the render thread never touches a freed player.
        void SetMediaPlayer(std::unique_ptr<IMediaPlayer> player);
        IMediaPlayer* GetMediaPlayer() const { return m_mediaPlayer.get(); }

    private:
        HWND m_hwnd;
        ComPtr<ID3D11Device> m_device;
        ComPtr<ID3D11DeviceContext> m_context;
        ComPtr<IDXGISwapChain> m_swapChain;
        ComPtr<ID3D11RenderTargetView> m_renderTargetView;

        bool CreateDeviceAndSwapChain();
        bool CreateRenderTarget();
        // Recomputes one D3D viewport per monitor (relative to the virtual-desktop origin)
        // so the same wallpaper is drawn full-size on every screen instead of stretched.
        void UpdateMonitorLayout();

        std::unique_ptr<IMediaPlayer> m_mediaPlayer;
        std::mutex m_mediaMutex; // guards m_mediaPlayer against concurrent swap/render

        std::vector<D3D11_VIEWPORT> m_viewports; // one per monitor
        int m_virtualLeft = 0;
        int m_virtualTop = 0;
    };
}
