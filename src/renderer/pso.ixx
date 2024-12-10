module;

#include <d3dx12.h>
#include <wil/com.h>

export module renderer.pso;

import std;
import system.logger;
import system.hash;
import system.asserts;

export namespace ysn
{
using PsoId = int64_t;

struct ComputePsoDesc
{
    void SetComputeShader(const void* binary, size_t size);
    void SetComputeShader(const D3D12_SHADER_BYTECODE& binary);

private:
    D3D12_COMPUTE_PIPELINE_STATE_DESC m_pso_desc = {};
};

struct GraphicsPsoDesc
{
    GraphicsPsoDesc(std::string name);

    PsoId GenerateId() const;
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& GetDesc() const;
    std::string GetName() const;
    std::wstring GetNameWString() const;
    ID3D12RootSignature* GetRootSignature() const;

    void SetRootSignature(ID3D12RootSignature* root_signature);

    void SetBlendState(const D3D12_BLEND_DESC& blend_desc);
    void SetRasterizerState(const D3D12_RASTERIZER_DESC& rasterizer_desc);
    void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& depth_stencil_desc);
    void SetSampleMask(UINT sample_mask);
    void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type);
    void SetDepthTargetFormat(DXGI_FORMAT dsv_format, UINT msaa_count = 1, UINT msaa_quality = 0);
    void SetRenderTargetFormat(DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format, UINT msaa_count = 1, UINT msaa_quality = 0);
    void SetRenderTargetFormats(UINT num_rtv, const DXGI_FORMAT* rtv_formats, DXGI_FORMAT dsv_format, UINT msaa_count = 1, UINT msaa_quality = 0);
    void SetInputLayout(UINT num_elements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs);
    void SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ib_props);

    // These const_casts shouldn't be necessary, but we need to fix the API to accept "const void* pShaderBytecode"
    void SetVertexShader(const void* binary, size_t size);
    void SetPixelShader(const void* binary, size_t size);
    void SetGeometryShader(const void* binary, size_t size);
    void SetHullShader(const void* binary, size_t size);
    void SetDomainShader(const void* binary, size_t size);

    void SetVertexShader(const D3D12_SHADER_BYTECODE& binary);
    void SetPixelShader(const D3D12_SHADER_BYTECODE& binary);
    void SetGeometryShader(const D3D12_SHADER_BYTECODE& binary);
    void SetHullShader(const D3D12_SHADER_BYTECODE& binary);
    void SetDomainShader(const D3D12_SHADER_BYTECODE& binary);

    // std::optional<PsoId> Build(wil::com_ptr<ID3D12Device5> device, std::unordered_map<PsoId, GraphicsPso>& pso_pool);

private:
    std::string m_name = "Unnamed PSO";

    // wil::com_ptr<ID3D12RootSignature> m_root_signature = nullptr;
    std::shared_ptr<D3D12_INPUT_ELEMENT_DESC> m_input_layout;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_pso_desc = {};
};

struct Pso
{
    PsoId pso_id = -1;
    wil::com_ptr<ID3D12RootSignature> root_signature;
    wil::com_ptr<ID3D12PipelineState> pso;
};

struct PsoStorage
{
    std::optional<PsoId> CreateGraphicsPso(wil::com_ptr<ID3D12Device5> device, const GraphicsPsoDesc& pso_desc);
    // std::optional<PsoId> CreateComputePso();

    std::optional<Pso> GetPso(PsoId pso_id);
    // std::optional<GraphicsPso> GetGraphicsPso();

    // void DeletePso();

    std::unordered_map<PsoId, Pso> m_graphics_pso_pool;
    // std::unordered_map<PsoId, ComputePso> m_compute_pso_pool;
};
} // namespace ysn

module :private;

namespace ysn
{
GraphicsPsoDesc::GraphicsPsoDesc(std::string name)
{
    m_name = name;
}

PsoId GraphicsPsoDesc::GenerateId() const
{
    auto pso_desc_copy = m_pso_desc;

    pso_desc_copy.InputLayout.pInputElementDescs = nullptr;

    PsoId pso_id = HashState(&pso_desc_copy);
    pso_id = HashState(m_input_layout.get(), m_pso_desc.InputLayout.NumElements, pso_id);

    // m_pso_desc.InputLayout.pInputElementDescs = m_input_layout.get();

    return pso_id;
}

const D3D12_GRAPHICS_PIPELINE_STATE_DESC& GraphicsPsoDesc::GetDesc() const
{
    return m_pso_desc;
}

std::string GraphicsPsoDesc::GetName() const
{
    return m_name;
}

std::wstring GraphicsPsoDesc::GetNameWString() const
{
    std::wstring name(m_name.begin(), m_name.end());
    return name;
}

ID3D12RootSignature* GraphicsPsoDesc::GetRootSignature() const
{
    return m_pso_desc.pRootSignature;
}

void GraphicsPsoDesc::SetRootSignature(ID3D12RootSignature* root_signature)
{
    m_pso_desc.pRootSignature = root_signature;
    AssertMsg(m_pso_desc.pRootSignature != nullptr, "Root signature can't be nullptr");
}

void GraphicsPsoDesc::SetBlendState(const D3D12_BLEND_DESC& blend_desc)
{
    m_pso_desc.BlendState = blend_desc;
}

void GraphicsPsoDesc::SetRasterizerState(const D3D12_RASTERIZER_DESC& rasterizer_desc)
{
    m_pso_desc.RasterizerState = rasterizer_desc;
}

void GraphicsPsoDesc::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& depth_stencil_desc)
{
    m_pso_desc.DepthStencilState = depth_stencil_desc;
}

void GraphicsPsoDesc::SetSampleMask(UINT sample_mask)
{
    m_pso_desc.SampleMask = sample_mask;
}

void GraphicsPsoDesc::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type)
{
    AssertMsg(topology_type != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED, "Can't draw with undefined topology");
    m_pso_desc.PrimitiveTopologyType = topology_type;
}

void GraphicsPsoDesc::SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ib_props)
{
    m_pso_desc.IBStripCutValue = ib_props;
}

void GraphicsPsoDesc::SetDepthTargetFormat(DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
    SetRenderTargetFormats(0, nullptr, DSVFormat, MsaaCount, MsaaQuality);
}

void GraphicsPsoDesc::SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
    SetRenderTargetFormats(1, &RTVFormat, DSVFormat, MsaaCount, MsaaQuality);
}

void GraphicsPsoDesc::SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
    AssertMsg(NumRTVs == 0 || RTVFormats != nullptr, "Null format array conflicts with non-zero length");
    for (UINT i = 0; i < NumRTVs; ++i)
    {
        AssertMsg(RTVFormats[i] != DXGI_FORMAT_UNKNOWN, "RTV Format can't be unknown!");
        m_pso_desc.RTVFormats[i] = RTVFormats[i];
    }
    // for (UINT i = NumRTVs; i < m_pso_desc.NumRenderTargets; ++i)
    //	m_pso_desc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
    m_pso_desc.NumRenderTargets = NumRTVs;
    m_pso_desc.DSVFormat = DSVFormat;
    m_pso_desc.SampleDesc.Count = MsaaCount;
    m_pso_desc.SampleDesc.Quality = MsaaQuality;
}

void GraphicsPsoDesc::SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs)
{
    m_pso_desc.InputLayout.NumElements = NumElements;

    if (NumElements > 0)
    {
        D3D12_INPUT_ELEMENT_DESC* new_elements = (D3D12_INPUT_ELEMENT_DESC*)malloc(sizeof(D3D12_INPUT_ELEMENT_DESC) * NumElements);
        memcpy(new_elements, pInputElementDescs, NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
        m_input_layout.reset((D3D12_INPUT_ELEMENT_DESC*)new_elements);

        m_pso_desc.InputLayout.pInputElementDescs = m_input_layout.get();
    }
    else
    {
        m_input_layout = nullptr;
    }
}

void GraphicsPsoDesc::SetVertexShader(const void* Binary, size_t Size)
{
    m_pso_desc.VS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
}

void GraphicsPsoDesc::SetPixelShader(const void* Binary, size_t Size)
{
    m_pso_desc.PS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
}

void GraphicsPsoDesc::SetGeometryShader(const void* Binary, size_t Size)
{
    m_pso_desc.GS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
}

void GraphicsPsoDesc::SetHullShader(const void* Binary, size_t Size)
{
    m_pso_desc.HS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
}

void GraphicsPsoDesc::SetDomainShader(const void* Binary, size_t Size)
{
    m_pso_desc.DS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
}

void GraphicsPsoDesc::SetVertexShader(const D3D12_SHADER_BYTECODE& Binary)
{
    m_pso_desc.VS = Binary;
}

void GraphicsPsoDesc::SetPixelShader(const D3D12_SHADER_BYTECODE& Binary)
{
    m_pso_desc.PS = Binary;
}

void GraphicsPsoDesc::SetGeometryShader(const D3D12_SHADER_BYTECODE& Binary)
{
    m_pso_desc.GS = Binary;
}

void GraphicsPsoDesc::SetHullShader(const D3D12_SHADER_BYTECODE& Binary)
{
    m_pso_desc.HS = Binary;
}

void GraphicsPsoDesc::SetDomainShader(const D3D12_SHADER_BYTECODE& Binary)
{
    m_pso_desc.DS = Binary;
}

std::optional<PsoId> PsoStorage::CreateGraphicsPso(wil::com_ptr<ID3D12Device5> device, const GraphicsPsoDesc& pso_desc)
{
    const PsoId pso_id = pso_desc.GenerateId();

    bool should_create_new = !m_graphics_pso_pool.contains(pso_id);

    if (should_create_new)
    {
        // YSN_ASSERT(pso_ref->DepthStencilState.DepthEnable != (m_pso_desc.DSVFormat == DXGI_FORMAT_UNKNOWN));

        Pso new_pso;

        const auto res = device->CreateGraphicsPipelineState(&pso_desc.GetDesc(), IID_PPV_ARGS(&new_pso.pso));

        if (res != S_OK)
        {
            LogError << "Can't compile pso '" << pso_desc.GetName() << "' " << std::to_string(pso_id) << " \n";
            return std::nullopt;
        }

        LogInfo << "Compiled new pso " << pso_desc.GetName() << " " << std::to_string(pso_id) << " \n";

        new_pso.pso_id = pso_id;
        new_pso.root_signature.attach(pso_desc.GetRootSignature());

#ifndef YSN_RELEASE
        new_pso.pso->SetName(pso_desc.GetNameWString().c_str());
#endif

        m_graphics_pso_pool[pso_id] = new_pso;
    }
    else
    {
        LogInfo << "Cache hit for pso: " << pso_desc.GetName() << " " << std::to_string(pso_id) << " \n";
    }

    return pso_id;
}

std::optional<Pso> PsoStorage::GetPso(PsoId pso_id)
{
    const bool is_found = m_graphics_pso_pool.contains(pso_id);

    if (is_found)
    {
        return m_graphics_pso_pool[pso_id];
    }
    else
    {
        return std::nullopt;
    }
}
} // namespace ysn