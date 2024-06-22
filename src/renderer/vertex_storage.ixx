module;

#include <d3dx12.h>
#include <DirectXMath.h>

export module renderer.vertex_storage;

import <vector>;

export namespace ysn
{
using namespace DirectX;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 tangent;
    XMFLOAT2 uv0;

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
    XMFLOAT3 pos;
    XMFLOAT2 tex_coord;

    static std::vector<D3D12_INPUT_ELEMENT_DESC> GetVertexLayoutDesc();
};

struct VertexStorage
{
    // uint32_t GetVertexBufferSize();

    // std::vector<VertexPosTexCoord> vertex_data;
    // GpuResource gpu_vertex_buffer;

    //// Multiple sources per channel?
};
} // namespace ysn

module :private;

namespace ysn
{
std::vector<D3D12_INPUT_ELEMENT_DESC> ysn::VertexPosTexCoord::GetVertexLayoutDesc()
{
    std::vector<D3D12_INPUT_ELEMENT_DESC> elements_desc;

    D3D12_INPUT_ELEMENT_DESC position = {};
    position.SemanticName = "POSITION";
    position.Format = DXGI_FORMAT_R32G32B32_FLOAT;
    position.InputSlot = 0;
    position.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    position.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    D3D12_INPUT_ELEMENT_DESC texcoord = {};
    texcoord.SemanticName = "TEXCOORD";
    texcoord.Format = DXGI_FORMAT_R32G32_FLOAT;
    texcoord.InputSlot = 0;
    texcoord.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    texcoord.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    elements_desc.push_back(position);
    elements_desc.push_back(texcoord);

    return elements_desc;
}
} // namespace ysn
