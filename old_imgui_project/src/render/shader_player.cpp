#include "shader_player.h"
#include <d3dcompiler.h>
#include <vector>
#include <fstream>
#include <sstream>

#pragma comment(lib, "d3dcompiler.lib")

namespace Render {

    struct Vertex {
        float x, y, z;
        float u, v;
    };

    ShaderPlayer::ShaderPlayer() {
        LARGE_INTEGER li;
        QueryPerformanceFrequency(&li);
        m_timerFreq = li.QuadPart;
    }

    ShaderPlayer::~ShaderPlayer() {
        Cleanup();
    }

    bool ShaderPlayer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
        m_device = device;
        m_context = context;

        if (!SetupBaseShaders()) return false;
        if (!SetupQuad()) return false;

        // Create constant buffer
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(ShaderToyCB);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
        return SUCCEEDED(hr);
    }

    bool ShaderPlayer::LoadMedia(const std::wstring& filePath) {
        if (!CompilePixelShader(filePath)) {
            return false;
        }

        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        m_startTime = li.QuadPart;
        m_currentTime = 0.0f;
        m_isPlaying = true;
        
        return true;
    }

    bool ShaderPlayer::CompilePixelShader(const std::wstring& filePath) {
        ComPtr<ID3DBlob> psBlob, errorBlob;
        
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG;
#endif

        // Note: We use mainImage as the entry point, just like Shadertoy!
        HRESULT hr = D3DCompileFromFile(filePath.c_str(), nullptr, nullptr, "main", "ps_5_0", flags, 0, &psBlob, &errorBlob);
        
        if (FAILED(hr)) {
            if (errorBlob) {
                OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            }
            return false;
        }

        hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
        return SUCCEEDED(hr);
    }

    void ShaderPlayer::Play() { 
        if (!m_isPlaying) {
            LARGE_INTEGER li;
            QueryPerformanceCounter(&li);
            // Adjust start time so the animation resumes from where it paused
            LONGLONG elapsedPause = li.QuadPart - m_pauseTime;
            m_startTime += elapsedPause;
            m_isPlaying = true; 
        }
    }

    void ShaderPlayer::Pause() { 
        if (m_isPlaying) {
            LARGE_INTEGER li;
            QueryPerformanceCounter(&li);
            m_pauseTime = li.QuadPart;
            m_isPlaying = false; 
        }
    }

    void ShaderPlayer::Stop() { 
        m_isPlaying = false; 
        m_currentTime = 0.0f;
    }

    bool ShaderPlayer::UpdateFrame() {
        if (!m_isPlaying) return false;

        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        m_currentTime = (float)((double)(li.QuadPart - m_startTime) / m_timerFreq);
        
        // Shader animations always update
        return true;
    }

    void ShaderPlayer::Render() {
        if (!m_pixelShader || !m_vertexShader) return;

        // Get viewport to set iResolution
        UINT numViewports = 1;
        D3D11_VIEWPORT vp;
        m_context->RSGetViewports(&numViewports, &vp);

        // Update constant buffer
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            ShaderToyCB* cb = (ShaderToyCB*)mapped.pData;
            cb->iResolution[0] = vp.Width;
            cb->iResolution[1] = vp.Height;
            cb->iResolution[2] = vp.Width / vp.Height;
            cb->iTime = m_currentTime;
            
            // Basic mouse implementation (optional enhancement later)
            POINT p;
            GetCursorPos(&p);
            cb->iMouse[0] = (float)p.x;
            cb->iMouse[1] = (float)(vp.Height - p.y); // Invert Y
            cb->iMouse[2] = 0.0f;
            cb->iMouse[3] = 0.0f;
            
            m_context->Unmap(m_constantBuffer.Get(), 0);
        }

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
        m_context->IASetInputLayout(m_inputLayout.Get());
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
        
        m_context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

        m_context->Draw(4, 0);
    }

    void ShaderPlayer::Cleanup() {
        m_pixelShader.Reset();
    }

    bool ShaderPlayer::SetupBaseShaders() {
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

        ComPtr<ID3DBlob> vsBlob, errorBlob;
        if (FAILED(D3DCompile(vertexShaderCode, strlen(vertexShaderCode), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob))) {
            return false;
        }

        if (FAILED(m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader))) return false;

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        if (FAILED(m_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout))) return false;

        return true;
    }

    bool ShaderPlayer::SetupQuad() {
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
