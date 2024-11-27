#pragma once

#include <d3dx12.h>
#include <wil/com.h>

namespace ysn
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
        const D3D12_INDIRECT_ARGUMENT_DESC& GetDesc( void ) const;
    protected:
        D3D12_INDIRECT_ARGUMENT_DESC m_indirect_parameter;
    };

    class CommandSignature
    {
    public:

    private:
        wil::com_ptr<ID3D12CommandSignature> m_signature;
    };
}