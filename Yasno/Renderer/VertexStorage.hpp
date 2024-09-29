#pragma once

#include <vector>

#include <d3dx12.h>
#include <DirectXMath.h>

#include <Renderer/GpuResource.hpp>

namespace ysn
{
    struct VertexPosTexCoord
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT2 tex_coord;
        
        static std::vector<D3D12_INPUT_ELEMENT_DESC> GetVertexLayoutDesc();
    };

    struct VertexStorage
    {
        uint32_t GetVertexBufferSize();

        std::vector<VertexPosTexCoord> vertex_data;
        GpuResource gpu_vertex_buffer;
    };
}