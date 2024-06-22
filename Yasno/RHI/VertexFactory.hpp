#pragma once

#include <d3dx12.h>
#include <DirectXMath.h>

namespace ysn
{
    class VertexPosTexCoord
    {
    public:
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT2 tex_coord;
        
        static std::vector<D3D12_INPUT_ELEMENT_DESC> GetVertexLayoutDesc();
    };
    
}