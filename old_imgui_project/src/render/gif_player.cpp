#include "gif_player.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace Render {

    struct Vertex {
        float x, y, z;
        float u, v;
    };

    GifPlayer::GifPlayer() {
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        m_timerFreq = li.QuadPart;
    }

    GifPlayer::~GifPlayer() {
        Cleanup();
    }

    bool GifPlayer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
        m_device = device;
        m_context = context;

        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wicFactory));
        if (FAILED(hr)) return false;

        if (!SetupShaders()) return false;
        if (!SetupQuad()) return false;

        return true;
    }

    bool GifPlayer::LoadMedia(const std::wstring& filePath) {
        if (!m_wicFactory) return false;

        HRESULT hr = m_wicFactory->CreateDecoderFromFilename(
            filePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &m_decoder);
        if (FAILED(hr)) return false;

        hr = m_decoder->GetFrameCount(&m_frameCount);
        if (FAILED(hr) || m_frameCount == 0) return false;

        // Get Logical Screen Size
        m_width = 0;
        m_height = 0;
        ComPtr<IWICMetadataQueryReader> metadataReader;
        if (SUCCEEDED(m_decoder->GetMetadataQueryReader(&metadataReader))) {
            PROPVARIANT prop;
            PropVariantInit(&prop);
            if (SUCCEEDED(metadataReader->GetMetadataByName(L"/logscrdesc/Width", &prop))) {
                if (prop.vt == VT_UI2) m_width = prop.uiVal;
                PropVariantClear(&prop);
            }
            if (SUCCEEDED(metadataReader->GetMetadataByName(L"/logscrdesc/Height", &prop))) {
                if (prop.vt == VT_UI2) m_height = prop.uiVal;
                PropVariantClear(&prop);
            }
        }

        if (m_width == 0 || m_height == 0) {
            ComPtr<IWICBitmapFrameDecode> firstFrame;
            hr = m_decoder->GetFrame(0, &firstFrame);
            if (FAILED(hr)) return false;
            hr = firstFrame->GetSize(&m_width, &m_height);
            if (FAILED(hr)) return false;
        }

        m_canvas.assign(m_width * m_height * 4, 0);
        m_previousCanvas.assign(m_width * m_height * 4, 0);
        m_prevDisposal = 0;

        if (!CreateFrameTexture(m_width, m_height)) return false;

        m_currentFrame = 0;
        if (!LoadFrame(0)) return false;

        m_lastFrameTime = 0;
        m_isPlaying = true;
        return true;
    }

    bool GifPlayer::LoadFrame(UINT32 frameIndex) {
        ComPtr<IWICBitmapFrameDecode> frame;
        HRESULT hr = m_decoder->GetFrame(frameIndex, &frame);
        if (FAILED(hr)) return false;

        // Dispose previous frame
        if (m_prevDisposal == 2) { // Restore to background
            for (UINT y = 0; y < m_prevHeight; y++) {
                for (UINT x = 0; x < m_prevWidth; x++) {
                    UINT px = m_prevLeft + x;
                    UINT py = m_prevTop + y;
                    if (px < m_width && py < m_height) {
                        UINT idx = (py * m_width + px) * 4;
                        m_canvas[idx] = 0;
                        m_canvas[idx+1] = 0;
                        m_canvas[idx+2] = 0;
                        m_canvas[idx+3] = 0;
                    }
                }
            }
        } else if (m_prevDisposal == 3) { // Restore to previous
            m_canvas = m_previousCanvas;
        }

        // Get frame dimensions and offsets
        UINT frameWidth = 0, frameHeight = 0;
        frame->GetSize(&frameWidth, &frameHeight);

        UINT left = 0, top = 0;
        UINT disposal = 0;
        ComPtr<IWICMetadataQueryReader> frameMetadata;
        if (SUCCEEDED(frame->GetMetadataQueryReader(&frameMetadata))) {
            PROPVARIANT prop;
            PropVariantInit(&prop);
            if (SUCCEEDED(frameMetadata->GetMetadataByName(L"/imgdesc/Left", &prop))) {
                if (prop.vt == VT_UI2) left = prop.uiVal;
                PropVariantClear(&prop);
            }
            if (SUCCEEDED(frameMetadata->GetMetadataByName(L"/imgdesc/Top", &prop))) {
                if (prop.vt == VT_UI2) top = prop.uiVal;
                PropVariantClear(&prop);
            }
            if (SUCCEEDED(frameMetadata->GetMetadataByName(L"/grctext/Disposal", &prop))) {
                if (prop.vt == VT_UI1) disposal = prop.bVal;
                PropVariantClear(&prop);
            }
        }

        // Save current canvas if we need to restore it later
        if (disposal == 3) {
            m_previousCanvas = m_canvas;
        }

        ComPtr<IWICFormatConverter> converter;
        hr = m_wicFactory->CreateFormatConverter(&converter);
        if (FAILED(hr)) return false;

        hr = converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.f,
            WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) return false;

        UINT stride = frameWidth * 4;
        UINT bufferSize = stride * frameHeight;
        std::vector<BYTE> buffer(bufferSize);

        hr = converter->CopyPixels(nullptr, stride, bufferSize, buffer.data());
        if (FAILED(hr)) return false;

        // Blend onto canvas
        for (UINT y = 0; y < frameHeight; y++) {
            for (UINT x = 0; x < frameWidth; x++) {
                UINT px = left + x;
                UINT py = top + y;
                if (px < m_width && py < m_height) {
                    UINT srcIdx = (y * frameWidth + x) * 4;
                    UINT dstIdx = (py * m_width + px) * 4;
                    BYTE alpha = buffer[srcIdx + 3];
                    if (alpha > 0) {
                        m_canvas[dstIdx] = buffer[srcIdx];
                        m_canvas[dstIdx+1] = buffer[srcIdx+1];
                        m_canvas[dstIdx+2] = buffer[srcIdx+2];
                        m_canvas[dstIdx+3] = buffer[srcIdx+3];
                    }
                }
            }
        }

        // Update tracking vars
        m_prevLeft = left;
        m_prevTop = top;
        m_prevWidth = frameWidth;
        m_prevHeight = frameHeight;
        m_prevDisposal = disposal;

        // Upload to texture
        D3D11_MAPPED_SUBRESOURCE mapped;
        hr = m_context->Map(m_frameTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            BYTE* dst = (BYTE*)mapped.pData;
            UINT canvasStride = m_width * 4;
            for (UINT row = 0; row < m_height; row++) {
                memcpy(dst + row * mapped.RowPitch, m_canvas.data() + row * canvasStride, canvasStride);
            }
            m_context->Unmap(m_frameTexture.Get(), 0);
        }

        m_currentFrameDuration = GetFrameDuration(frameIndex);
        return true;
    }

    double GifPlayer::GetFrameDuration(UINT32 frameIndex) {
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(m_decoder->GetFrame(frameIndex, &frame))) return 0.1;

        ComPtr<IWICMetadataQueryReader> metadataReader;
        if (FAILED(frame->GetMetadataQueryReader(&metadataReader))) return 0.1;

        PROPVARIANT propValue;
        PropVariantInit(&propValue);

        double duration = 0.1; // Default 100ms
        if (SUCCEEDED(metadataReader->GetMetadataByName(L"/grctext/Delay", &propValue))) {
            if (propValue.vt == VT_UI2) {
                // Delay is in 10ms units
                duration = propValue.uiVal * 0.01;
            }
            PropVariantClear(&propValue);
        }
        
        // Handle minimum delay for poorly encoded GIFs
        if (duration < 0.02) duration = 0.1;
        
        return duration;
    }

    void GifPlayer::Play() { m_isPlaying = true; }
    void GifPlayer::Pause() { m_isPlaying = false; }
    void GifPlayer::Stop() { m_isPlaying = false; m_currentFrame = 0; }

    bool GifPlayer::UpdateFrame() {
        if (!m_isPlaying || m_frameCount == 0) return false;

        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        LONGLONG currentTime = li.QuadPart;

        if (m_lastFrameTime != 0) {
            double elapsedSeconds = (double)(currentTime - m_lastFrameTime) / m_timerFreq;
            if (elapsedSeconds < m_currentFrameDuration) {
                return false;
            }
        }

        m_currentFrame = (m_currentFrame + 1) % m_frameCount;
        LoadFrame(m_currentFrame);
        m_lastFrameTime = currentTime;
        return true;
    }

    void GifPlayer::Render() {
        if (!m_frameSRV || !m_vertexShader) return;

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
        m_context->IASetInputLayout(m_inputLayout.Get());
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
        m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
        m_context->PSSetShaderResources(0, 1, m_frameSRV.GetAddressOf());

        m_context->Draw(4, 0);
    }

    void GifPlayer::Cleanup() {
        m_frameTexture.Reset();
        m_frameSRV.Reset();
        m_decoder.Reset();
    }

    bool GifPlayer::CreateFrameTexture(UINT32 width, UINT32 height) {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        if (FAILED(m_device->CreateTexture2D(&desc, nullptr, &m_frameTexture))) return false;
        return SUCCEEDED(m_device->CreateShaderResourceView(m_frameTexture.Get(), nullptr, &m_frameSRV));
    }

    bool GifPlayer::SetupShaders() {
        // Reuse the exact same shader code as VideoPlayer
        const char* vertexShaderCode = R"(
            struct VS_INPUT { float3 pos : POSITION; float2 tex : TEXCOORD; };
            struct PS_INPUT { float4 pos : SV_POSITION; float2 tex : TEXCOORD; };
            PS_INPUT main(VS_INPUT input) {
                PS_INPUT output;
                output.pos = float4(input.pos, 1.0f);
                output.tex = input.tex;
                return output;
            }
        )";

        const char* pixelShaderCode = R"(
            Texture2D shaderTexture : register(t0);
            SamplerState sampleType : register(s0);
            struct PS_INPUT { float4 pos : SV_POSITION; float2 tex : TEXCOORD; };
            float4 main(PS_INPUT input) : SV_TARGET {
                return shaderTexture.Sample(sampleType, input.tex);
            }
        )";

        ComPtr<ID3DBlob> vsBlob, psBlob;
        if (FAILED(D3DCompile(vertexShaderCode, strlen(vertexShaderCode), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsBlob, nullptr))) return false;
        if (FAILED(D3DCompile(pixelShaderCode, strlen(pixelShaderCode), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, nullptr))) return false;

        if (FAILED(m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader))) return false;
        if (FAILED(m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader))) return false;

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        if (FAILED(m_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout))) return false;

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        return SUCCEEDED(m_device->CreateSamplerState(&samplerDesc, &m_samplerState));
    }

    bool GifPlayer::SetupQuad() {
        Vertex vertices[] = {
            { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
            { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
            {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
            {  1.0f,  1.0f, 0.0f, 1.0f, 0.0f }
        };

        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = sizeof(vertices);
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = vertices;

        return SUCCEEDED(m_device->CreateBuffer(&bufferDesc, &initData, &m_vertexBuffer));
    }
}
