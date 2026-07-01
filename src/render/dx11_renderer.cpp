#include "dx11_renderer.h"
#include "media_player.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace Render {

    DX11Renderer::DX11Renderer() : m_hwnd(nullptr) {}

    DX11Renderer::~DX11Renderer() {
        Cleanup();
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

        UpdateMonitorLayout();

        // The active media player is supplied later via SetMediaPlayer (App::LoadMedia),
        // which initializes it against this device/context.
        return true;
    }

    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) {
        auto* rects = reinterpret_cast<std::vector<RECT>*>(lParam);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hMonitor, &mi)) {
            rects->push_back(mi.rcMonitor);
        }
        return TRUE;
    }

    void DX11Renderer::UpdateMonitorLayout() {
        // The wallpaper window/swap chain spans the whole virtual desktop. Each monitor
        // gets a viewport placed at its offset from the virtual-desktop origin so the
        // media renders at native size per monitor (not stretched across all of them).
        m_virtualLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
        m_virtualTop  = GetSystemMetrics(SM_YVIRTUALSCREEN);

        std::vector<RECT> rects;
        EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)&rects);

        m_viewports.clear();
        for (const auto& r : rects) {
            D3D11_VIEWPORT vp = {};
            vp.TopLeftX = (FLOAT)(r.left - m_virtualLeft);
            vp.TopLeftY = (FLOAT)(r.top - m_virtualTop);
            vp.Width = (FLOAT)(r.right - r.left);
            vp.Height = (FLOAT)(r.bottom - r.top);
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            m_viewports.push_back(vp);
        }
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

        // Clear to black so any uncovered area blends with the desktop instead of flashing a color.
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

        std::lock_guard<std::mutex> lock(m_mediaMutex);
        if (m_mediaPlayer) {
            // Advance the frame once, then draw it into each monitor's viewport.
            m_mediaPlayer->UpdateFrame();
            if (m_viewports.empty()) {
                m_mediaPlayer->Render();
            } else {
                for (const auto& vp : m_viewports) {
                    m_context->RSSetViewports(1, &vp);
                    m_mediaPlayer->Render();
                }
            }
        }
    }

    HRESULT DX11Renderer::Present() {
        if (m_swapChain) {
            return m_swapChain->Present(1, 0); // VSync enabled
        }
        return S_OK;
    }

    HRESULT DX11Renderer::TestOcclusion() {
        if (m_swapChain) {
            // DXGI_PRESENT_TEST does not render; it only reports S_OK or DXGI_STATUS_OCCLUDED.
            return m_swapChain->Present(0, DXGI_PRESENT_TEST);
        }
        return S_OK;
    }

    bool DX11Renderer::Resize(UINT width, UINT height) {
        if (!m_swapChain || width == 0 || height == 0) return false;

        m_context->OMSetRenderTargets(0, nullptr, nullptr);
        m_renderTargetView.Reset();

        HRESULT hr = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) return false;

        if (!CreateRenderTarget()) return false;

        D3D11_VIEWPORT vp = {};
        vp.Width = (FLOAT)width;
        vp.Height = (FLOAT)height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);

        UpdateMonitorLayout();
        return true;
    }

    void DX11Renderer::SetMediaPlayer(std::unique_ptr<IMediaPlayer> player) {
        std::lock_guard<std::mutex> lock(m_mediaMutex);
        if (m_mediaPlayer) {
            m_mediaPlayer->Stop();
            m_mediaPlayer->Cleanup();
        }
        m_mediaPlayer = std::move(player);
    }
}
