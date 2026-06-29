#include "dx11_renderer.h"
#include "video_player.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace Render {

    DX11Renderer::DX11Renderer() : m_hwnd(nullptr), m_mediaPlayer(new VideoPlayer()) {}

    DX11Renderer::~DX11Renderer() {
        Cleanup();
        delete m_mediaPlayer;
    }

    bool DX11Renderer::Initialize(HWND hwnd) {
        m_hwnd = hwnd;
        if (!CreateDeviceAndSwapChain()) return false;
        if (!CreateRenderTarget()) return false;

        // Set the viewport
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        D3D11_VIEWPORT vp;
        vp.Width = (FLOAT)(rc.right - rc.left);
        vp.Height = (FLOAT)(rc.bottom - rc.top);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        m_context->RSSetViewports(1, &vp);

        if (!m_mediaPlayer->Initialize(m_device.Get(), m_context.Get())) return false;

        return true;
    }

    void DX11Renderer::Cleanup() {
        if (m_context) m_context->ClearState();
        m_renderTargetView.Reset();
        m_swapChain.Reset();
        m_context.Reset();
        m_device.Reset();
    }

    bool DX11Renderer::CreateDeviceAndSwapChain() {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        UINT width = rc.right - rc.left;
        UINT height = rc.bottom - rc.top;

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2; // Double buffering
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = m_hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        UINT createDeviceFlags = 0;
#ifdef _DEBUG
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        UINT numFeatureLevels = ARRAYSIZE(featureLevels);

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
            featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &sd,
            &m_swapChain, &m_device, nullptr, &m_context);

        return SUCCEEDED(hr);
    }

    bool DX11Renderer::CreateRenderTarget() {
        ComPtr<ID3D11Texture2D> pBackBuffer;
        HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
        if (FAILED(hr)) return false;

        hr = m_device->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &m_renderTargetView);
        if (FAILED(hr)) return false;

        m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), nullptr);
        return true;
    }

    void DX11Renderer::RenderFrame() {
        if (!m_context || !m_renderTargetView) return;

        // Ensure the render target is bound. In FLIP_DISCARD swap chains, it can become unbound after Present.
        m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), nullptr);

        // Clear to dark red to help debug if rendering fails
        float clearColor[4] = { 0.2f, 0.0f, 0.0f, 1.0f };
        m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

        if (m_mediaPlayer) {
            m_mediaPlayer->UpdateFrame();
            m_mediaPlayer->Render();
        }
    }

    void DX11Renderer::Present() {
        if (m_swapChain) {
            m_swapChain->Present(1, 0); // VSync enabled
        }
    }
}
