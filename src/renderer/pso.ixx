module;

#include <d3dx12.h>
#include <wil/com.h>
#include <dxcapi.h>

export module renderer.pso;

import std;
import system.logger;
import system.hash;
import system.asserts;
import renderer.shader_storage;
import renderer.dx_types;

export namespace ysn
{
using PsoId = int64_t;

// Rename to PsoObject
struct Pso
{
    // Rename to id and others
    PsoId pso_id = -1;
    wil::com_ptr<ID3D12RootSignature> root_signature;
    wil::com_ptr<ID3D12PipelineState> pso;
};
} // namespace ysn

namespace ysn
{
class PsoDesc
{
public:
    virtual PsoId GenerateId() const = 0;
    virtual void SetRootSignature(ID3D12RootSignature* root_signature) = 0;
    virtual bool BuildShaders(wil::com_ptr<DxDevice> device, std::shared_ptr<ShaderStorage> shader_storage) = 0;

    std::string GetName() const;
    std::wstring GetNameWString() const;
    virtual ID3D12RootSignature* GetRootSignature() const = 0;

protected:
    std::string m_name = "Unnamed PSO";
};
} // namespace ysn

export namespace ysn
{
class ComputePsoDesc : public PsoDesc
{
public:
    ComputePsoDesc(std::string name);

    PsoId GenerateId() const override;
    bool BuildShaders(wil::com_ptr<DxDevice> device, std::shared_ptr<ShaderStorage> shader_storage) override;

    const D3D12_COMPUTE_PIPELINE_STATE_DESC& GetDesc() const;

    void AddShader(const ShaderCompileParameters& shader_parameter);
    void SetRootSignature(ID3D12RootSignature* root_signature) override;
    ID3D12RootSignature* GetRootSignature() const override;

private:
    ShaderCompileParameters m_shader;
    D3D12_COMPUTE_PIPELINE_STATE_DESC m_pso_desc = {};
};

class GraphicsPsoDesc : public PsoDesc
{
public:
    GraphicsPsoDesc(std::string name);

    PsoId GenerateId() const override;
    bool BuildShaders(wil::com_ptr<DxDevice> device, std::shared_ptr<ShaderStorage> shader_storage) override;

    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& GetDesc() const;
    ID3D12RootSignature* GetRootSignature() const override;

    void SetRootSignature(ID3D12RootSignature* root_signature) override;
    void SetBlendState(const D3D12_BLEND_DESC& blend_desc);
    void SetRasterizerState(const D3D12_RASTERIZER_DESC& rasterizer_desc);
    void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& depth_stencil_desc);
    void SetSampleMask(UINT sample_mask);
    void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type);
    void SetDepthTargetFormat(DXGI_FORMAT dsv_format, UINT msaa_count = 1, UINT msaa_quality = 0);
    void SetRenderTargetFormat(DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format, UINT msaa_count = 1, UINT msaa_quality = 0);
    void SetRenderTargetFormats(UINT num_rtv, const DXGI_FORMAT* rtv_formats, DXGI_FORMAT dsv_format, UINT msaa_count = 1, UINT msaa_quality = 0);
    void SetInputLayout(UINT num_elements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs);
    void SetInputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& elements);
    void SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ib_props);
    void AddShader(const ShaderCompileParameters& shader_parameter);

private:
    std::vector<ShaderCompileParameters> m_shaders_to_compile;
    std::shared_ptr<D3D12_INPUT_ELEMENT_DESC> m_input_layout;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_pso_desc = {};
};

class PsoStorage
{
public:
    bool Initialize();

    std::optional<PsoId> BuildPso(wil::com_ptr<DxDevice> device, PsoDesc* pso_desc);

    std::optional<Pso> GetPso(PsoId pso_id);
    std::unordered_map<PsoId, Pso> m_graphics_pso_pool;

    // Hide after raytracing manual shader compilation is fixed
    std::shared_ptr<ShaderStorage> GetShaderStorage()
    {
        return m_shader_storage;
    }

private:
    std::shared_ptr<ShaderStorage> m_shader_storage;
};

} // namespace ysn

module :private;

namespace ysn
{
ComputePsoDesc::ComputePsoDesc(std::string name)
{
    m_name = name;
}

void ComputePsoDesc::AddShader(const ShaderCompileParameters& shader_parameter)
{
    if (shader_parameter.type != ShaderType::Compute)
    {
        LogError << "Wrong shader specified" << m_name << "\n";
        AssertMsg(false, "Wrong shader specified");
        return; // TODO: enhance handling of the error and type checks
    }
    m_shader = shader_parameter;
}

void ComputePsoDesc::SetRootSignature(ID3D12RootSignature* root_signature)
{
    m_pso_desc.pRootSignature = root_signature;
    AssertMsg(m_pso_desc.pRootSignature != nullptr, "Root signature can't be nullptr");
}

PsoId ComputePsoDesc::GenerateId() const
{
    PsoId pso_id = HashState(&m_pso_desc);

    return pso_id;
}

bool ComputePsoDesc::BuildShaders(wil::com_ptr<DxDevice> device, std::shared_ptr<ShaderStorage> shader_storage)
{
    const auto& compiled_shader_res = shader_storage->CompileShader(m_shader);

    if (!compiled_shader_res.has_value())
    {
        LogError << "Can't compile shader\n";
        return false;
    }

    wil::com_ptr<IDxcBlob> blob = compiled_shader_res.value();
    const auto bytecode = CD3DX12_SHADER_BYTECODE(const_cast<void*>(blob->GetBufferPointer()), blob->GetBufferSize());

    m_pso_desc.CS = bytecode;

    return true;
}

ID3D12RootSignature* ComputePsoDesc::GetRootSignature() const
{
    return m_pso_desc.pRootSignature;
}

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

bool GraphicsPsoDesc::BuildShaders(wil::com_ptr<DxDevice> device, std::shared_ptr<ShaderStorage> shader_storage)
{
    for (const auto& shader : m_shaders_to_compile)
    {
        const auto& compiled_shader_res = shader_storage->CompileShader(shader);

        if (!compiled_shader_res.has_value())
        {
            LogError << "Can't compile shader\n";
            return false;
        }

        wil::com_ptr<IDxcBlob> blob = compiled_shader_res.value();
        const auto bytecode = CD3DX12_SHADER_BYTECODE(const_cast<void*>(blob->GetBufferPointer()), blob->GetBufferSize());

        switch (shader.type)
        {
        case ShaderType::Vertex:
            m_pso_desc.VS = bytecode;
            break;
        case ShaderType::Pixel:
            m_pso_desc.PS = bytecode;
            break;
            // TODO: finish and sanitize
        }
    }

    return true;
}

const D3D12_GRAPHICS_PIPELINE_STATE_DESC& GraphicsPsoDesc::GetDesc() const
{
    return m_pso_desc;
}

const D3D12_COMPUTE_PIPELINE_STATE_DESC& ComputePsoDesc::GetDesc() const
{
    return m_pso_desc;
}

std::string PsoDesc::GetName() const
{
    return m_name;
}

std::wstring PsoDesc::GetNameWString() const
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

void GraphicsPsoDesc::SetInputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& elements)
{
    SetInputLayout(static_cast<UINT>(elements.size()), elements.data());
}

void GraphicsPsoDesc::AddShader(const ShaderCompileParameters& shader_parameter)
{
    m_shaders_to_compile.push_back(shader_parameter);
}

bool PsoStorage::Initialize()
{
    m_shader_storage = std::make_shared<ShaderStorage>();

    if (!m_shader_storage->Initialize())
    {
        LogError << "Can't create shader storage\n";
        return false;
    }

    return true;
}

std::optional<PsoId> PsoStorage::BuildPso(wil::com_ptr<DxDevice> device, PsoDesc* pso_desc)
{
    // TODO: Check result
    pso_desc->BuildShaders(device, m_shader_storage);

    // Compile shaders
    const PsoId pso_id = pso_desc->GenerateId();

    bool should_create_new = !m_graphics_pso_pool.contains(pso_id);

    if (should_create_new)
    {
        // YSN_ASSERT(pso_ref->DepthStencilState.DepthEnable != (m_pso_desc.DSVFormat == DXGI_FORMAT_UNKNOWN));

        Pso new_pso;

        GraphicsPsoDesc* graphics_pso = dynamic_cast<GraphicsPsoDesc*>(pso_desc);
        ComputePsoDesc* compute_pso = dynamic_cast<ComputePsoDesc*>(pso_desc);

        if (!graphics_pso && !compute_pso)
        {
            LogError << "Can't compile PSO '" << pso_desc->GetName() << "' " << std::to_string(pso_id) << " \n";
            return std::nullopt;
        }

        HRESULT res = S_FALSE;

        if (graphics_pso != nullptr)
        {
            res = device->CreateGraphicsPipelineState(&graphics_pso->GetDesc(), IID_PPV_ARGS(&new_pso.pso));
        }
        if (compute_pso != nullptr)
        {
            res = device->CreateComputePipelineState(&compute_pso->GetDesc(), IID_PPV_ARGS(&new_pso.pso));
        }

        if (res != S_OK)
        {
            LogError << "Can't compile PSO '" << pso_desc->GetName() << "' " << std::to_string(pso_id) << " \n";
            return std::nullopt;
        }

        LogInfo << "Compiled new pso " << pso_desc->GetName() << " " << std::to_string(pso_id) << " \n";

        new_pso.pso_id = pso_id;
        new_pso.root_signature.attach(pso_desc->GetRootSignature());

#ifndef YSN_RELEASE
        new_pso.pso->SetName(pso_desc->GetNameWString().c_str());
#endif

        m_graphics_pso_pool[pso_id] = new_pso;
    }
    else
    {
        LogInfo << "Cache hit for pso: " << pso_desc->GetName() << " " << std::to_string(pso_id) << " \n";
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