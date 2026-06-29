#pragma once

#include <windows.h>
#include <d3d11.h>
#include <string>

namespace Render {

    class IMediaPlayer {
    public:
        virtual ~IMediaPlayer() = default;

        virtual bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context) = 0;
        virtual bool LoadMedia(const std::wstring& filePath) = 0;
        
        virtual void Play() = 0;
        virtual void Pause() = 0;
        virtual void Stop() = 0;

        virtual bool UpdateFrame() = 0;
        virtual void Render() = 0;
        
        virtual void Cleanup() = 0;
    };

}
