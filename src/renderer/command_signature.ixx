module;

#include <d3dx12.h>
#include <wil/com.h>

export module renderer.command_signature;

export namespace ysn
{
class IndirectParameter
{
    friend class CommandSignature;

public:
    IndirectParameter();
    void Draw();
    void DrawIndexed();
    void Dispatch();
    void VertexBufferView(uint32_t slot);
    void IndexBufferView();
    void Constant(uint32_t root_parameter_index, uint32_t dest_offset_in_32_bit_values, uint32_t num_32_bit_values_to_set);
    void ConstantBufferView(uint32_t root_parameter_index);
    void ShaderResourceView(uint32_t root_parameter_index);
    void UnorderedAccessView(uint32_t root_parameter_index);
    const D3D12_INDIRECT_ARGUMENT_DESC& GetDesc(void) const;

protected:
    D3D12_INDIRECT_ARGUMENT_DESC m_indirect_parameter;
};

class CommandSignature
{
public:
private:
    wil::com_ptr<ID3D12CommandSignature> m_signature;
};
} // namespace ysn

module :private;

namespace ysn
{
IndirectParameter::IndirectParameter()
{
    m_indirect_parameter.Type = (D3D12_INDIRECT_ARGUMENT_TYPE)0xFFFFFFFF;
}

void IndirectParameter::Draw()
{
    m_indirect_parameter.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
}

void IndirectParameter::DrawIndexed()
{
    m_indirect_parameter.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
}

void IndirectParameter::Dispatch()
{
    m_indirect_parameter.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
}

void IndirectParameter::VertexBufferView(uint32_t slot)
{
    m_indirect_parameter.Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
    m_indirect_parameter.VertexBuffer.Slot = (UINT)slot;
}

void IndirectParameter::IndexBufferView()
{
    m_indirect_parameter.Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
}

void IndirectParameter::Constant(uint32_t root_parameter_index, uint32_t dest_offset_in_32_bit_values, uint32_t num_32_bit_values_to_set)
{
    m_indirect_parameter.Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
    m_indirect_parameter.Constant.RootParameterIndex = (UINT)root_parameter_index;
    m_indirect_parameter.Constant.DestOffsetIn32BitValues = (UINT)dest_offset_in_32_bit_values;
    m_indirect_parameter.Constant.Num32BitValuesToSet = (UINT)num_32_bit_values_to_set;
}

void IndirectParameter::ConstantBufferView(uint32_t root_parameter_index)
{
    m_indirect_parameter.Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
    m_indirect_parameter.ConstantBufferView.RootParameterIndex = (UINT)root_parameter_index;
}

void IndirectParameter::ShaderResourceView(uint32_t root_parameter_index)
{
    m_indirect_parameter.Type = D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW;
    m_indirect_parameter.ShaderResourceView.RootParameterIndex = (UINT)root_parameter_index;
}

void IndirectParameter::UnorderedAccessView(uint32_t root_parameter_index)
{
    m_indirect_parameter.Type = D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW;
    m_indirect_parameter.UnorderedAccessView.RootParameterIndex = (UINT)root_parameter_index;
}

const D3D12_INDIRECT_ARGUMENT_DESC& IndirectParameter::GetDesc(void) const
{
    return m_indirect_parameter;
}
} // namespace ysn