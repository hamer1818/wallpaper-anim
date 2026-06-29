#pragma once

#include <windows.h>
#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

#include "media_player.h"

namespace Render {

    class VideoPlayer : public IMediaPlayer {
    public:
        VideoPlayer();
        virtual ~VideoPlayer();

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
        bool LoadMedia(const std::wstring& filePath) override;
        void Play() override;
        void Pause() override;
        void Stop() override;

        // Updates the video frame if a new one is available
        bool UpdateFrame() override;

        // Renders the current frame to the screen
        void Render() override;

        void Cleanup() override;

        float GetFPS() const { return m_fps; }

    private:
        ComPtr<ID3D11Device> m_device;
        ComPtr<ID3D11DeviceContext> m_context;

        ComPtr<IMFSourceReader> m_sourceReader;

        bool m_isPlaying = false;
        UINT32 m_videoWidth = 0;
        UINT32 m_videoHeight = 0;

        float m_fps = 30.0f;
        LONGLONG m_lastFrameTime = 0;
        LONGLONG m_timerFreq = 0;

        // Texture for the current frame (RGB32, dynamic)
        ComPtr<ID3D11Texture2D> m_frameTexture;
        ComPtr<ID3D11ShaderResourceView> m_frameSRV;

        // Shader resources
        ComPtr<ID3D11VertexShader> m_vertexShader;
        ComPtr<ID3D11PixelShader> m_pixelShader;
        ComPtr<ID3D11InputLayout> m_inputLayout;
        ComPtr<ID3D11Buffer> m_vertexBuffer;
        ComPtr<ID3D11SamplerState> m_samplerState;

        bool SetupShaders();
        bool SetupQuad();
        bool CreateFrameTexture(UINT32 width, UINT32 height);
    };

}
