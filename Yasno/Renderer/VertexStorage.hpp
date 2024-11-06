#pragma once

#include <vector>

#include <d3dx12.h>
#include <DirectXMath.h>

#include <Renderer/GpuResource.hpp>

namespace ysn
{
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT4 tangent;
        DirectX::XMFLOAT2 uv0;

        Vertex& operator=(const Vertex& v)
        {
            position = v.position;
            normal = v.normal;
            tangent = v.tangent;
            uv0 = v.uv0;
            return *this;
        }
    };

    struct VertexPosTexCoord
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT2 tex_coord;
        
        static std::vector<D3D12_INPUT_ELEMENT_DESC> GetVertexLayoutDesc();
    };

    struct VertexStorage
    {
        //uint32_t GetVertexBufferSize();

        //std::vector<VertexPosTexCoord> vertex_data;
        //GpuResource gpu_vertex_buffer;

        //// Multiple sources per channel?
    };
}