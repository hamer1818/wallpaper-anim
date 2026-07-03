#include "video_player.h"
#include <d3dcompiler.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace Render {

    // ---- Embedded shaders (no external .hlsl file needed) ----
    // The pixel shader samples the NV12 luma (Y) and chroma (UV) planes and converts
    // YUV->RGB on the GPU, so the CPU never runs a color-conversion MFT or touches RGB
    // pixels. isBT709 picks the HD vs SD coefficient set.
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
        Texture2D<float>  texY  : register(t0);
        Texture2D<float2> texUV : register(t1);
        SamplerState sam0 : register(s0);
        cbuffer ColorCB : register(b0) {
            float isBT709;
            float3 _pad;
        };
        float4 PSMain(PS_INPUT input) : SV_TARGET {
            // NV12 is studio/limited range (Y in 16..235, UV in 16..240, /255).
            float y = (texY.Sample(sam0, input.tex) - 0.0625) * 1.164383;
            float2 uv = texUV.Sample(sam0, input.tex) - float2(0.5, 0.5);
            float u = uv.x;
            float v = uv.y;
            float3 rgb;
            if (isBT709 > 0.5) {
                rgb = float3(
                    y + 1.792741 * v,
                    y - 0.213249 * u - 0.532909 * v,
                    y + 2.112402 * u);
            } else {
                rgb = float3(
                    y + 1.596027 * v,
                    y - 0.391762 * u - 0.812968 * v,
                    y + 2.017232 * u);
            }
            return float4(saturate(rgb), 1.0);
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
        m_texY.Reset();
        m_srvY.Reset();
        m_texUV.Reset();
        m_srvUV.Reset();
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

        // Constant buffer carrying the YUV-matrix selector (16-byte aligned).
        struct ColorCB { float isBT709; float pad[3]; };
        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.ByteWidth = sizeof(ColorCB);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_device->CreateBuffer(&cbd, nullptr, &m_colorCB))) return false;

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

    bool VideoPlayer::CreateVideoTextures(UINT32 width, UINT32 height) {
        m_texY.Reset();
        m_srvY.Reset();
        m_texUV.Reset();
        m_srvUV.Reset();

        // Luma plane: full resolution, one 8-bit sample per pixel.
        D3D11_TEXTURE2D_DESC yd = {};
        yd.Width = width;
        yd.Height = height;
        yd.MipLevels = 1;
        yd.ArraySize = 1;
        yd.Format = DXGI_FORMAT_R8_UNORM;
        yd.SampleDesc.Count = 1;
        yd.Usage = D3D11_USAGE_DYNAMIC;
        yd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        yd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_device->CreateTexture2D(&yd, nullptr, &m_texY))) return false;
        if (FAILED(m_device->CreateShaderResourceView(m_texY.Get(), nullptr, &m_srvY))) return false;

        // Chroma plane: half resolution, interleaved U/V (2 bytes per texel).
        D3D11_TEXTURE2D_DESC uvd = yd;
        uvd.Width = (width + 1) / 2;
        uvd.Height = (height + 1) / 2;
        uvd.Format = DXGI_FORMAT_R8G8_UNORM;
        if (FAILED(m_device->CreateTexture2D(&uvd, nullptr, &m_texUV))) return false;
        if (FAILED(m_device->CreateShaderResourceView(m_texUV.Get(), nullptr, &m_srvUV))) return false;

        return true;
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
        // Request NV12: the native output of H.264 hardware decoders, so MF can decode
        // on the GPU and skip the CPU-side YUV->RGB conversion MFT. We convert to RGB in
        // the pixel shader instead.
        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);

        hr = m_sourceReader->SetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, mediaType.Get());
        if (FAILED(hr)) return false;

        ComPtr<IMFMediaType> outputType;
        hr = m_sourceReader->GetCurrentMediaType(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &outputType);
        if (FAILED(hr)) return false;

        hr = MFGetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, &m_videoWidth, &m_videoHeight);
        if (FAILED(hr)) return false;

        // The decoder may pad each row to an alignment boundary; honor the real stride so
        // rows don't skew. Falls back to the packed (== width) stride if unspecified.
        INT32 declaredStride = 0;
        if (SUCCEEDED(outputType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&declaredStride)) && declaredStride != 0) {
            m_srcStride = (UINT32)abs(declaredStride);
        } else {
            m_srcStride = m_videoWidth;
        }

        UINT32 num = 0, den = 1;
        if (SUCCEEDED(MFGetAttributeRatio(outputType.Get(), MF_MT_FRAME_RATE, &num, &den)) && num > 0 && den > 0) {
            m_fps = (float)num / den;
        } else {
            m_fps = 30.0f;
        }

        // Pick the YUV->RGB matrix: prefer the stream's declared value, else fall back to
        // the usual convention (BT.709 for HD, BT.601 for SD).
        m_isBT709 = (m_videoHeight > 576);
        UINT32 yuvMatrix = 0;
        if (SUCCEEDED(outputType->GetUINT32(MF_MT_YUV_MATRIX, &yuvMatrix))) {
            m_isBT709 = (yuvMatrix != MFVideoTransferMatrix_BT601);
        }

        if (!CreateVideoTextures(m_videoWidth, m_videoHeight)) return false;

        // Push the matrix selection into the pixel shader's constant buffer.
        D3D11_MAPPED_SUBRESOURCE cbMap;
        if (SUCCEEDED(m_context->Map(m_colorCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMap))) {
            float* f = (float*)cbMap.pData;
            f[0] = m_isBT709 ? 1.0f : 0.0f;
            f[1] = f[2] = f[3] = 0.0f;
            m_context->Unmap(m_colorCB.Get(), 0);
        }

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
        if (!m_isPlaying || !m_sourceReader || !m_texY) return false;

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

        // Prefer a single decoder buffer (usually exposes the real 2D layout); fall back
        // to a contiguous copy for the rare multi-buffer sample.
        ComPtr<IMFMediaBuffer> buffer;
        DWORD bufCount = 0;
        if (SUCCEEDED(sample->GetBufferCount(&bufCount)) && bufCount == 1) {
            hr = sample->GetBufferByIndex(0, &buffer);
        } else {
            hr = sample->ConvertToContiguousBuffer(&buffer);
        }
        if (FAILED(hr) || !buffer) return false;

        // Resolve the real NV12 layout. Decoders often pad the luma plane's *height* to an
        // alignment boundary (e.g. 1080->1088), so the chroma plane does not start at
        // stride*videoHeight. Reading the actual pitch + buffer length via IMF2DBuffer
        // lets us locate the chroma plane exactly instead of guessing (a wrong offset
        // shows up as a green band where chroma reads zero).
        BYTE* srcData = nullptr;      // first luma scanline
        UINT  srcStride = m_srcStride; // real luma/chroma row pitch
        UINT  lumaRows = m_videoHeight; // rows from luma start to chroma start
        bool  used2D = false;

        ComPtr<IMF2DBuffer2> buf2d;
        if (SUCCEEDED(buffer.As(&buf2d))) {
            BYTE* scan0 = nullptr; LONG pitch = 0; BYTE* bufStart = nullptr; DWORD bufLen = 0;
            if (SUCCEEDED(buf2d->Lock2DSize(MF2DBuffer_LockFlags_Read, &scan0, &pitch, &bufStart, &bufLen)) && pitch > 0) {
                srcData = scan0;
                srcStride = (UINT)pitch;
                // Total buffer holds luma + half-height chroma at the same pitch:
                // bufLen = pitch * lumaRows * 3/2  ->  lumaRows = (bufLen / pitch) * 2/3.
                lumaRows = (bufLen / srcStride) * 2 / 3;
                used2D = true;
            }
        }
        if (!used2D) {
            // Fallback: plain locked buffer, assume packed layout after the luma plane.
            if (FAILED(buffer->Lock(&srcData, nullptr, nullptr))) return false;
        }

        const UINT copyBytes = m_videoWidth; // valid bytes per row (same for Y and UV planes)
        const UINT chromaHeight = (m_videoHeight + 1) / 2;

        // Upload luma plane -> R8 texture.
        D3D11_MAPPED_SUBRESOURCE mappedY;
        if (SUCCEEDED(m_context->Map(m_texY.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedY))) {
            BYTE* dst = (BYTE*)mappedY.pData;
            for (UINT row = 0; row < m_videoHeight; row++) {
                memcpy(dst + row * mappedY.RowPitch, srcData + (size_t)row * srcStride, copyBytes);
            }
            m_context->Unmap(m_texY.Get(), 0);
        }

        // Upload interleaved chroma plane -> R8G8 texture (starts after the luma plane).
        BYTE* srcUV = srcData + (size_t)srcStride * lumaRows;
        D3D11_MAPPED_SUBRESOURCE mappedUV;
        if (SUCCEEDED(m_context->Map(m_texUV.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedUV))) {
            BYTE* dst = (BYTE*)mappedUV.pData;
            for (UINT row = 0; row < chromaHeight; row++) {
                memcpy(dst + row * mappedUV.RowPitch, srcUV + (size_t)row * srcStride, copyBytes);
            }
            m_context->Unmap(m_texUV.Get(), 0);
        }

        m_lastFrameTime = currentTime; // Update timer ONLY if we successfully processed a frame
        if (used2D) buf2d->Unlock2D();
        else buffer->Unlock();
        return true;
    }

    void VideoPlayer::Render() {
        if (!m_srvY || !m_srvUV || !m_vertexShader) return;

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
        m_context->IASetInputLayout(m_inputLayout.Get());
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
        m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
        m_context->PSSetConstantBuffers(0, 1, m_colorCB.GetAddressOf());

        ID3D11ShaderResourceView* srvs[2] = { m_srvY.Get(), m_srvUV.Get() };
        m_context->PSSetShaderResources(0, 2, srvs);

        m_context->Draw(4, 0);
    }
}
