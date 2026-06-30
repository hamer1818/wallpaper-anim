#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>

using Microsoft::WRL::ComPtr;

namespace Render {
    class DX11Renderer {
    public:
        DX11Renderer();
        ~DX11Renderer();

        bool Initialize(HWND hwnd);
        void Cleanup();

        void RenderFrame();
        void Present();

        ID3D11Device* GetDevice() const { return m_device.Get(); }
        ID3D11DeviceContext* GetContext() const { return m_context.Get(); }
        
        void SetMediaPlayer(class IMediaPlayer* player) { m_mediaPlayer = player; }
        class IMediaPlayer* GetMediaPlayer() const { return m_mediaPlayer; }

    private:
        HWND m_hwnd;
        ComPtr<ID3D11Device> m_device;
        ComPtr<ID3D11DeviceContext> m_context;
        ComPtr<IDXGISwapChain> m_swapChain;
        ComPtr<ID3D11RenderTargetView> m_renderTargetView;

        bool CreateDeviceAndSwapChain();
        bool CreateRenderTarget();

        class IMediaPlayer* m_mediaPlayer;
    };
}
