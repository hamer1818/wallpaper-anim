#include "preview_player.h"
#include <windows.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace Utils {

    PreviewPlayer::PreviewPlayer() {
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        m_timerFreq = li.QuadPart;
    }

    PreviewPlayer::~PreviewPlayer() {
        Stop();
    }

    bool PreviewPlayer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
        m_device = device;
        m_context = context;
        return true;
    }

    bool PreviewPlayer::CreateFrameTexture(UINT32 width, UINT32 height) {
        m_frameTexture.Reset();
        m_frameSRV.Reset();

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // Matches MFVideoFormat_RGB32
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_frameTexture);
        if (FAILED(hr)) return false;

        hr = m_device->CreateShaderResourceView(m_frameTexture.Get(), nullptr, &m_frameSRV);
        return SUCCEEDED(hr);
    }

    bool PreviewPlayer::Start(const std::wstring& mediaPath) {
        // If already playing the same file, do nothing
        if (m_isPlaying && m_currentPath == mediaPath) return true;

        // Stop any existing preview
        Stop();

        // Check extension — only preview video files
        std::wstring ext;
        size_t dotPos = mediaPath.find_last_of(L".");
        if (dotPos != std::wstring::npos) {
            ext = mediaPath.substr(dotPos + 1);
            for (auto& c : ext) c = towlower(c);
        }
        // Only support video formats for preview
        if (ext != L"mp4" && ext != L"avi" && ext != L"mkv" && ext != L"webm" && ext != L"mov" && ext != L"gif") {
            return false;
        }

        // Initialize MF
        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) return false;
        m_mfInitialized = true;

        // Create source reader
        ComPtr<IMFAttributes> attributes;
        hr = MFCreateAttributes(&attributes, 2);
        if (FAILED(hr)) return false;

        attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

        hr = MFCreateSourceReaderFromURL(mediaPath.c_str(), attributes.Get(), &m_sourceReader);
        if (FAILED(hr)) {
            Stop();
            return false;
        }

        // Request RGB32 output
        ComPtr<IMFMediaType> mediaType;
        hr = MFCreateMediaType(&mediaType);
        if (FAILED(hr)) { Stop(); return false; }

        mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);

        hr = m_sourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, mediaType.Get());
        if (FAILED(hr)) { Stop(); return false; }

        // Get dimensions
        ComPtr<IMFMediaType> outputType;
        hr = m_sourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &outputType);
        if (FAILED(hr)) { Stop(); return false; }

        hr = MFGetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, &m_videoWidth, &m_videoHeight);
        if (FAILED(hr)) { Stop(); return false; }

        // Create texture
        if (!CreateFrameTexture(m_videoWidth, m_videoHeight)) {
            Stop();
            return false;
        }

        m_currentPath = mediaPath;
        m_isPlaying = true;
        m_lastFrameTime = 0;

        return true;
    }

    void PreviewPlayer::Stop() {
        m_isPlaying = false;
        m_sourceReader.Reset();
        m_frameTexture.Reset();
        m_frameSRV.Reset();
        m_currentPath.clear();
        m_videoWidth = 0;
        m_videoHeight = 0;
        m_lastFrameTime = 0;

        if (m_mfInitialized) {
            MFShutdown();
            m_mfInitialized = false;
        }
    }

    bool PreviewPlayer::Update() {
        if (!m_isPlaying || !m_sourceReader || !m_frameTexture || !m_context) return false;

        // Throttle to ~15 FPS
        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        LONGLONG currentTime = li.QuadPart;

        if (m_lastFrameTime != 0) {
            double elapsed = (double)(currentTime - m_lastFrameTime) / m_timerFreq;
            if (elapsed < (1.0 / m_previewFPS)) {
                return false;
            }
        }

        // Read next frame
        DWORD streamIndex, flags;
        LONGLONG timestamp;
        ComPtr<IMFSample> sample;

        HRESULT hr = m_sourceReader->ReadSample(
            (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0, &streamIndex, &flags, &timestamp, &sample);

        if (FAILED(hr)) return false;

        // End of stream or past 5 seconds — loop back to start
        if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) || timestamp >= PREVIEW_DURATION) {
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

            for (UINT row = 0; row < m_videoHeight; row++) {
                BYTE* srcRow = srcData + row * srcStride;
                memcpy(dst + row * mapped.RowPitch, srcRow, srcStride);
            }

            m_context->Unmap(m_frameTexture.Get(), 0);
        }

        m_lastFrameTime = currentTime;
        buffer->Unlock();
        return true;
    }

}
