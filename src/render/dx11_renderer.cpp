#include "dx11_renderer.h"
#include "media_player.h"
#include "../config.h"

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
        if (!CreateScaleBuffer()) return false;

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

    bool DX11Renderer::CreateScaleBuffer() {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = 16; // float4
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_device->CreateBuffer(&bd, nullptr, &m_scaleCB))) return false;

        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.DepthClipEnable = TRUE;
        rd.ScissorEnable = TRUE; // clip each monitor's draw to its own rect
        return SUCCEEDED(m_device->CreateRasterizerState(&rd, &m_scissorRaster));
    }

    void DX11Renderer::SetFitScale(const D3D11_VIEWPORT& vp, UINT contentW, UINT contentH) {
        float sx = 1.0f, sy = 1.0f;
        if (contentW > 0 && contentH > 0 && vp.Width > 0 && vp.Height > 0) {
            const float va = (float)contentW / (float)contentH; // content aspect
            const float ma = vp.Width / vp.Height;              // monitor aspect
            switch (Config::ConfigManager::GetInstance().GetConfig().fitMode) {
            case 0: // Fill: cover the monitor, cropping the overflow axis
                if (va > ma) sx = va / ma; else sy = ma / va;
                break;
            case 1: // Fit: letterbox so the whole frame is visible
                if (va > ma) sy = ma / va; else sx = va / ma;
                break;
            case 2: // Stretch: leave 1:1 (fills viewport exactly, may distort)
                break;
            case 3: // Center: draw at native pixel size, centered
                sx = (float)contentW / vp.Width;
                sy = (float)contentH / vp.Height;
                break;
            }
        }

        D3D11_MAPPED_SUBRESOURCE m;
        if (SUCCEEDED(m_context->Map(m_scaleCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
            float* f = (float*)m.pData;
            f[0] = sx; f[1] = sy; f[2] = 0.0f; f[3] = 0.0f;
            m_context->Unmap(m_scaleCB.Get(), 0);
        }
        m_context->VSSetConstantBuffers(1, 1, m_scaleCB.GetAddressOf());
    }

    void DX11Renderer::Cleanup() {
        if (m_context) m_context->ClearState();
        m_scaleCB.Reset();
        m_scissorRaster.Reset();
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

        UINT baseFlags = 0;
#ifdef _DEBUG
        baseFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        // VIDEO_SUPPORT lets Media Foundation hardware-decode straight onto this device
        // (zero-copy path in VideoPlayer). BGRA_SUPPORT is required alongside it.
        UINT createDeviceFlags = baseFlags | D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        UINT numFeatureLevels = ARRAYSIZE(featureLevels);

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
            featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &sd,
            &m_swapChain, &m_device, nullptr, &m_context);

        // If a driver rejects the video/BGRA flags, retry with the plain flag set so the
        // wallpaper still runs (VideoPlayer then falls back to its CPU upload path).
        if (FAILED(hr)) {
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, baseFlags,
                featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &sd,
                &m_swapChain, &m_device, nullptr, &m_context);
        }
        if (FAILED(hr)) return false;

        // MF decodes on its own worker threads using this device, so it must be
        // multithread-protected. Harmless if video-mode decoding is never used.
        ComPtr<ID3D10Multithread> mt;
        if (SUCCEEDED(m_device.As(&mt))) {
            mt->SetMultithreadProtected(TRUE);
        }
        return true;
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

    bool DX11Renderer::RenderFrame() {
        if (!m_context || !m_renderTargetView) return false;

        std::lock_guard<std::mutex> lock(m_mediaMutex);
        if (!m_mediaPlayer) return false;

        // Only do the (relatively expensive) clear + draw when the player actually has a
        // new frame. If UpdateFrame() reports nothing new, the back buffer already holds
        // the current frame, so we return false and the loop skips Present entirely.
        if (!m_mediaPlayer->UpdateFrame()) return false;

        // Ensure the render target is bound. In FLIP_DISCARD swap chains, it can become unbound after Present.
        m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), nullptr);

        // Clear to black so any uncovered area blends with the desktop instead of flashing a color.
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

        // Draw the frame into each monitor's viewport, scaled to preserve the media's
        // aspect ratio per the active fit mode. Scissor clipping keeps Fill/Center
        // overflow inside the monitor it belongs to.
        m_context->RSSetState(m_scissorRaster.Get());
        UINT cw = 0, ch = 0;
        m_mediaPlayer->GetContentSize(cw, ch);
        auto drawViewport = [&](const D3D11_VIEWPORT& vp) {
            m_context->RSSetViewports(1, &vp);
            D3D11_RECT scissor = {
                (LONG)vp.TopLeftX, (LONG)vp.TopLeftY,
                (LONG)(vp.TopLeftX + vp.Width), (LONG)(vp.TopLeftY + vp.Height)
            };
            m_context->RSSetScissorRects(1, &scissor);
            SetFitScale(vp, cw, ch);
            m_mediaPlayer->Render();
        };
        if (m_viewports.empty()) {
            D3D11_VIEWPORT vp;
            UINT n = 1;
            m_context->RSGetViewports(&n, &vp);
            drawViewport(vp);
        } else {
            for (const auto& vp : m_viewports) {
                drawViewport(vp);
            }
        }
        return true;
    }

    HRESULT DX11Renderer::Present(UINT syncInterval) {
        if (m_swapChain) {
            return m_swapChain->Present(syncInterval, 0); // VSync-paced by the hardware
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
