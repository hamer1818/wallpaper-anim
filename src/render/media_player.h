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

        // Native content size in pixels, used to preserve aspect ratio when fitting the
        // media to a monitor. Return (0, 0) for content that has no intrinsic size and
        // should always fill the viewport (e.g. a procedural shader).
        virtual void GetContentSize(UINT& width, UINT& height) const { width = 0; height = 0; }
        
        virtual void Cleanup() = 0;
    };

}
