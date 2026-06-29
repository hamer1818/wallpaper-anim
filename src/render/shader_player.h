#pragma once

#include "media_player.h"
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

namespace Render {

    class ShaderPlayer : public IMediaPlayer {
    public:
        ShaderPlayer();
        virtual ~ShaderPlayer();

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

        bool m_isPlaying = false;
        LONGLONG m_startTime = 0;
        LONGLONG m_pauseTime = 0;
        LONGLONG m_timerFreq = 0;
        float m_currentTime = 0.0f;

        // Shader resources
        ComPtr<ID3D11VertexShader> m_vertexShader;
        ComPtr<ID3D11PixelShader> m_pixelShader;
        ComPtr<ID3D11InputLayout> m_inputLayout;
        ComPtr<ID3D11Buffer> m_vertexBuffer;

        // Constant buffer for Shadertoy uniforms
        struct ShaderToyCB {
            float iResolution[3];
            float iTime;
            float iMouse[4];
        };
        ComPtr<ID3D11Buffer> m_constantBuffer;

        bool SetupBaseShaders();
        bool SetupQuad();
        bool CompilePixelShader(const std::wstring& filePath);
    };

}
