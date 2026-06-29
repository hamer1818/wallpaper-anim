#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <string>

using Microsoft::WRL::ComPtr;

namespace Utils {
    class TextureLoader {
    public:
        static bool LoadTextureFromFile(const std::wstring& filename, ID3D11Device* d3dDevice, ComPtr<ID3D11ShaderResourceView>& out_srv, int& out_width, int& out_height);
    };
}
