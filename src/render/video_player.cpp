#include "video_player.h"
#include <d3dcompiler.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace Render {

    // ---- Embedded shaders (no external .hlsl file needed) ----
    static const char* g_shaderSource = R"(
        struct VS_INPUT {
            float3 pos : POSITION;
            float2 tex : TEXCOORD;
        };
        struct PS_INPUT {
            float4 pos : SV_POSITION;
            float2 tex : TEXCOORD;
        };
        PS_INPUT VSMain(VS_INPUT input) {
            PS_INPUT output;
            output.pos = float4(input.pos, 1.0f);
            output.tex = input.tex;
            return output;
        }
        Texture2D tex0 : register(t0);
        SamplerState sam0 : register(s0);
        float4 PSMain(PS_INPUT input) : SV_TARGET {
            return tex0.Sample(sam0, input.tex);
        }
    )";

    struct Vertex {
        float x, y, z;
        float u, v;
    };

    VideoPlayer::VideoPlayer() {
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        m_timerFreq = li.QuadPart;
    }

    VideoPlayer::~VideoPlayer() {
        Cleanup();
        MFShutdown();
    }

    void VideoPlayer::Cleanup() {
        m_sourceReader.Reset();
        m_frameTexture.Reset();
        m_frameSRV.Reset();
    }

    bool VideoPlayer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
        m_device = device;
        m_context = context;

        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) {
            OutputDebugStringW(L"[VideoPlayer] MFStartup failed\n");
            return false;
        }

        if (!SetupShaders()) {
            OutputDebugStringW(L"[VideoPlayer] SetupShaders failed\n");
            return false;
        }
        if (!SetupQuad()) {
            OutputDebugStringW(L"[VideoPlayer] SetupQuad failed\n");
            return false;
        }

        return true;
    }

    bool VideoPlayer::SetupShaders() {
        ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
        HRESULT hr;

        // Compile vertex shader from embedded string
        hr = D3DCompile(g_shaderSource, strlen(g_shaderSource), "embedded",
            nullptr, nullptr, "VSMain", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            OutputDebugStringW(L"[VideoPlayer] VS compile failed\n");
            return false;
        }

        // Compile pixel shader from embedded string
        hr = D3DCompile(g_shaderSource, strlen(g_shaderSource), "embedded",
            nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &psBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            OutputDebugStringW(L"[VideoPlayer] PS compile failed\n");
            return false;
        }

        m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
        m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        m_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout);

        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        m_device->CreateSamplerState(&sampDesc, &m_samplerState);

        return true;
    }

    bool VideoPlayer::SetupQuad() {
        // Fullscreen quad (triangle strip)
        Vertex vertices[] = {
            { -1.0f,  1.0f, 0.0f,   0.0f, 0.0f },  // top-left
            {  1.0f,  1.0f, 0.0f,   1.0f, 0.0f },  // top-right
            { -1.0f, -1.0f, 0.0f,   0.0f, 1.0f },  // bottom-left
            {  1.0f, -1.0f, 0.0f,   1.0f, 1.0f },  // bottom-right
        };

        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(vertices);
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = vertices;

        return SUCCEEDED(m_device->CreateBuffer(&bd, &initData, &m_vertexBuffer));
    }

    bool VideoPlayer::CreateFrameTexture(UINT32 width, UINT32 height) {
        m_frameTexture.Reset();
        m_frameSRV.Reset();

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // Matches MFVideoFormat_RGB32
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_frameTexture);
        if (FAILED(hr)) return false;

        hr = m_device->CreateShaderResourceView(m_frameTexture.Get(), nullptr, &m_frameSRV);
        return SUCCEEDED(hr);
    }

    bool VideoPlayer::LoadMedia(const std::wstring& filePath) {
        // Check if file exists
        DWORD attrib = GetFileAttributesW(filePath.c_str());
        if (attrib == INVALID_FILE_ATTRIBUTES) return false;

        ComPtr<IMFAttributes> attributes;
        HRESULT hr = MFCreateAttributes(&attributes, 1);
        if (FAILED(hr)) return false;

        attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

        hr = MFCreateSourceReaderFromURL(filePath.c_str(), attributes.Get(), &m_sourceReader);
        if (FAILED(hr)) return false;

        ComPtr<IMFMediaType> mediaType;
        hr = MFCreateMediaType(&mediaType);
        if (FAILED(hr)) return false;

        mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);

        hr = m_sourceReader->SetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, mediaType.Get());
        if (FAILED(hr)) return false;

        ComPtr<IMFMediaType> outputType;
        hr = m_sourceReader->GetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &outputType);
        if (FAILED(hr)) return false;

        hr = MFGetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, &m_videoWidth, &m_videoHeight);
        if (FAILED(hr)) return false;

        UINT32 num = 0, den = 1;
        if (SUCCEEDED(MFGetAttributeRatio(outputType.Get(), MF_MT_FRAME_RATE, &num, &den)) && num > 0 && den > 0) {
            m_fps = (float)num / den;
        } else {
            m_fps = 30.0f;
        }

        if (!CreateFrameTexture(m_videoWidth, m_videoHeight)) return false;

        // Probe-decode one frame. Some codecs (notably VP9/AV1 on stock Windows 10)
        // let the topology build but fail at actual decode time, which would otherwise
        // show nothing with no error. Failing here lets the caller report it instead.
        {
            DWORD probeStream = 0, probeFlags = 0;
            LONGLONG probeTs = 0;
            ComPtr<IMFSample> probeSample;
            HRESULT probeHr = m_sourceReader->ReadSample(
                (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0, &probeStream, &probeFlags, &probeTs, &probeSample);
            if (FAILED(probeHr)) return false;

            // Rewind so playback still starts from the first frame.
            PROPVARIANT var = {};
            var.vt = VT_I8;
            var.hVal.QuadPart = 0;
            m_sourceReader->SetCurrentPosition(GUID_NULL, var);
        }

        m_lastFrameTime = 0;
        m_isPlaying = true;
        return true;
    }

    void VideoPlayer::Play()  { m_isPlaying = true; }
    void VideoPlayer::Pause() { m_isPlaying = false; }
    void VideoPlayer::Stop()  { m_isPlaying = false; }

    bool VideoPlayer::UpdateFrame() {
        if (!m_isPlaying || !m_sourceReader || !m_frameTexture) return false;

        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        LONGLONG currentTime = li.QuadPart;

        if (m_lastFrameTime != 0) {
            double elapsedSeconds = (double)(currentTime - m_lastFrameTime) / m_timerFreq;
            if (elapsedSeconds < (1.0 / m_fps)) {
                return false; // Not time for the next frame yet
            }
        }

        DWORD streamIndex, flags;
        LONGLONG timestamp;
        ComPtr<IMFSample> sample;

        HRESULT hr = m_sourceReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0, &streamIndex, &flags, &timestamp, &sample);

        if (FAILED(hr)) return false;

        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            PROPVARIANT var = {};
            var.vt = VT_I8;
            var.hVal.QuadPart = 0;
            m_sourceReader->SetCurrentPosition(GUID_NULL, var);
            return false;
        }

        if (!sample) return false;

        ComPtr<IMFMediaBuffer> buffer;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (FAILED(hr)) return false;

        BYTE* srcData = nullptr;
        DWORD srcLength = 0;
        hr = buffer->Lock(&srcData, nullptr, &srcLength);
        if (FAILED(hr)) return false;

        // Copy pixel data into D3D11 dynamic texture
        D3D11_MAPPED_SUBRESOURCE mapped;
        hr = m_context->Map(m_frameTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            UINT srcStride = m_videoWidth * 4;
            BYTE* dst = (BYTE*)mapped.pData;

            // Copy row by row (strides might differ between MF buffer and D3D11 texture)
            for (UINT row = 0; row < m_videoHeight; row++) {
                BYTE* srcRow = srcData + row * srcStride;
                memcpy(dst + row * mapped.RowPitch, srcRow, srcStride);
            }

            m_context->Unmap(m_frameTexture.Get(), 0);
        }

        m_lastFrameTime = currentTime; // Update timer ONLY if we successfully processed a frame
        buffer->Unlock();
        return true;
    }

    void VideoPlayer::Render() {
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
}
