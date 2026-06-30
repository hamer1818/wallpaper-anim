#pragma once

#include "media_player.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace Render {

    class GifPlayer : public IMediaPlayer {
    public:
        GifPlayer();
        virtual ~GifPlayer();

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) override;
        bool LoadMedia(const std::wstring& filePath) override;

        void Play() override;
        void Pause() override;
        void Stop() override;

        bool UpdateFrame() override;
        void Render() override;
        void Cleanup() override;

    private:
        ComPtr<ID3D11Device> m_device;
        ComPtr<ID3D11DeviceContext> m_context;

        ComPtr<IWICImagingFactory> m_wicFactory;
        ComPtr<IWICBitmapDecoder> m_decoder;

        bool m_isPlaying = false;
        UINT32 m_width = 0;
        UINT32 m_height = 0;
        UINT32 m_frameCount = 0;
        UINT32 m_currentFrame = 0;

        LONGLONG m_lastFrameTime = 0;
        LONGLONG m_timerFreq = 0;
        double m_currentFrameDuration = 0.0;

        std::vector<BYTE> m_canvas;
        std::vector<BYTE> m_previousCanvas;
        UINT32 m_prevLeft = 0, m_prevTop = 0, m_prevWidth = 0, m_prevHeight = 0;
        UINT32 m_prevDisposal = 0;

        // Texture for the current frame
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
        
        bool LoadFrame(UINT32 frameIndex);
        double GetFrameDuration(UINT32 frameIndex);
    };

}
