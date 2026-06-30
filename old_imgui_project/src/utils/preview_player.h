#pragma once
#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <string>
#include <atomic>

using Microsoft::WRL::ComPtr;

namespace Utils {

    // Lightweight video preview player for library hover previews.
    // Plays the first ~5 seconds of a video in a loop at low FPS,
    // rendering frames into a D3D11 texture suitable for ImGui::Image().
    class PreviewPlayer {
    public:
        PreviewPlayer();
        ~PreviewPlayer();

        // Initialize with the D3D11 device (call once)
        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

        // Start playing a preview for the given media file
        bool Start(const std::wstring& mediaPath);

        // Stop the current preview and free resources
        void Stop();

        // Update the preview frame (call each UI frame).
        // Returns true if the texture was updated.
        bool Update();

        // Get the current frame texture for ImGui::Image()
        ID3D11ShaderResourceView* GetFrameSRV() const { return m_frameSRV.Get(); }

        // Is currently playing a preview?
        bool IsPlaying() const { return m_isPlaying; }

        // Get the path of the currently playing media
        const std::wstring& GetCurrentPath() const { return m_currentPath; }

    private:
        bool CreateFrameTexture(UINT32 width, UINT32 height);

        ComPtr<ID3D11Device> m_device;
        ComPtr<ID3D11DeviceContext> m_context;
        ComPtr<IMFSourceReader> m_sourceReader;
        ComPtr<ID3D11Texture2D> m_frameTexture;
        ComPtr<ID3D11ShaderResourceView> m_frameSRV;

        std::wstring m_currentPath;
        bool m_isPlaying = false;
        bool m_mfInitialized = false;

        UINT32 m_videoWidth = 0;
        UINT32 m_videoHeight = 0;

        // Timing
        LONGLONG m_timerFreq = 0;
        LONGLONG m_lastFrameTime = 0;
        float m_previewFPS = 15.0f;

        // Loop control: restart after ~5 seconds
        static const LONGLONG PREVIEW_DURATION = 50000000LL; // 5 seconds in 100ns units
    };

}
