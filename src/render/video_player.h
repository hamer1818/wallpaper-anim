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

        void GetContentSize(UINT& width, UINT& height) const override { width = m_videoWidth; height = m_videoHeight; }

        void Cleanup() override;

        float GetFPS() const { return m_fps; }

    private:
        ComPtr<ID3D11Device> m_device;
        ComPtr<ID3D11DeviceContext> m_context;

        ComPtr<IMFSourceReader> m_sourceReader;

        // Zero-copy GPU decode: when the decoder can hand back D3D-backed NV12 samples,
        // we GPU-copy them straight into an SRV texture instead of the CPU memcpy path.
        ComPtr<IMFDXGIDeviceManager> m_deviceManager;
        bool m_gpuMode = false;
        ComPtr<ID3D11Texture2D> m_nv12Tex; // GPU-mode SRV-capable NV12 copy target
        // Decoder textures may be padded to alignment; sample only the valid region.
        float m_texScaleX = 1.0f;
        float m_texScaleY = 1.0f;

        bool m_isPlaying = false;
        UINT32 m_videoWidth = 0;
        UINT32 m_videoHeight = 0;
        UINT32 m_srcStride = 0; // luma row stride of the decoded NV12 buffer (may be padded)

        float m_fps = 30.0f;
        LONGLONG m_lastFrameTime = 0;
        LONGLONG m_timerFreq = 0;

        // Current frame as NV12 planes (dynamic): full-res luma (R8) + half-res
        // interleaved chroma (R8G8). The YUV->RGB conversion happens in the pixel shader
        // (on the GPU) instead of via a CPU color-conversion MFT, and we upload 1.5
        // bytes/pixel instead of RGB32's 4.
        ComPtr<ID3D11Texture2D> m_texY;
        ComPtr<ID3D11ShaderResourceView> m_srvY;
        ComPtr<ID3D11Texture2D> m_texUV;
        ComPtr<ID3D11ShaderResourceView> m_srvUV;

        // true = BT.709 (HD) YUV matrix, false = BT.601 (SD). Chosen from the stream's
        // MF_MT_YUV_MATRIX (falling back to a resolution heuristic).
        bool m_isBT709 = true;

        // Shader resources
        ComPtr<ID3D11VertexShader> m_vertexShader;
        ComPtr<ID3D11PixelShader> m_pixelShader;
        ComPtr<ID3D11InputLayout> m_inputLayout;
        ComPtr<ID3D11Buffer> m_vertexBuffer;
        ComPtr<ID3D11Buffer> m_colorCB; // holds the YUV-matrix selector for the pixel shader
        ComPtr<ID3D11Buffer> m_texCB;   // VS b2: texcoord scale to skip decoder padding
        ComPtr<ID3D11SamplerState> m_samplerState;

        bool SetupShaders();
        bool SetupQuad();
        bool CreateVideoTexturesCPU(UINT32 width, UINT32 height);
        bool CreateVideoTexturesGPU(UINT32 texW, UINT32 texH);
    };

}
