#include "VertexStorage.hpp"

namespace ysn
{
    std::vector<D3D12_INPUT_ELEMENT_DESC> ysn::VertexPosTexCoord::GetVertexLayoutDesc()
    {
        std::vector<D3D12_INPUT_ELEMENT_DESC> elements_desc;

        D3D12_INPUT_ELEMENT_DESC position = {};
        position.SemanticName       = "POSITION";
        position.Format             = DXGI_FORMAT_R32G32B32_FLOAT;
        position.InputSlot          = 0;
        position.AlignedByteOffset  = D3D12_APPEND_ALIGNED_ELEMENT;
        position.InputSlotClass     = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

        D3D12_INPUT_ELEMENT_DESC texcoord = {};
        texcoord.SemanticName       = "TEXCOORD";
        texcoord.Format             = DXGI_FORMAT_R32G32_FLOAT;
        texcoord.InputSlot          = 0;
        texcoord.AlignedByteOffset  = D3D12_APPEND_ALIGNED_ELEMENT;
        texcoord.InputSlotClass     = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

        elements_desc.push_back(position);
        elements_desc.push_back(texcoord);

        return elements_desc;
    }


    //uint32_t VertexStorage::GetVertexBufferSize()
    //{
    //    return 0;
    //}

}

