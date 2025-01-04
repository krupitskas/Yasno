module;

#include <d3dx12.h>
#include <wil/com.h>
#include <dxcapi.h>

export module renderer.pso;

import std;
import system.logger;
import system.hash;
import system.string_helpers;
import system.asserts;
import renderer.shader_storage;
import renderer.dx_types;

export namespace ysn
{
using PsoId = int64_t;   // Id can't change
using PsoHash = int64_t; // Hash can if we hotreload shaders and PSO will have another signature
} // namespace ysn

namespace ysn
{
static PsoId g_pso_id_counter = 0;

static PsoId GeneratePsoId()
{
    return g_pso_id_counter += 1;
}

enum class PsoType
{
    Compute,
    Graphics,
    Unknown
};

class PsoDesc
{
public:
    PsoDesc() = default;

    PsoDesc(PsoType type, std::string name) : type(type), m_name(name)
    {
    }

    virtual PsoHash GenerateHash() const
    {
        return 0;
    };
    virtual void SetRootSignature(ID3D12RootSignature* ) {};
    virtual std::optional<std::vector<ShaderHash>> BuildShaders(wil::com_ptr<DxDevice> device, std::shared_ptr<ShaderStorage> shader_storage) {

        return std::nullopt;
    }

    std::string GetName() const;
    std::wstring GetNameWString() const;
    virtual ID3D12RootSignature* GetRootSignature() const { return nullptr; };

    PsoType type = PsoType::Unknown;

protected:
    std::string m_name = "Unnamed PSO";
    D3D12_COMPUTE_PIPELINE_STATE_DESC m_compute_pso_desc = {};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_graphics_pso_desc = {};
};
} // namespace ysn

export namespace ysn
{
// Rename to PsoObject
class Pso
{
public:
    Pso() = default;
    // Rename to id and others
    PsoId pso_id = -1;
    wil::com_ptr<ID3D12RootSignature> root_signature;
    wil::com_ptr<ID3D12PipelineState> pso;
    PsoDesc pso_desc; // To allow hot reload
};

class ComputePsoDesc : public PsoDesc
{
public:
    ComputePsoDesc(PsoDesc desc) : PsoDesc(desc)
    {
    }

    ComputePsoDesc(std::string name) : PsoDesc(PsoType::Compute, name)
    {
    }

    PsoHash GenerateHash() const override;
    std::optional<std::vector<ShaderHash>> BuildShaders(wil::com_ptr<DxDevice> device, std::shared_ptr<ShaderStorage> shader_storage) override;

    const D3D12_COMPUTE_PIPELINE_STATE_DESC& GetDesc() const;

    void AddShader(const ShaderCompileParameters& shader_parameter);
    void SetRootSignature(ID3D12RootSignature* root_signature) override;
    ID3D12RootSignature* GetRootSignature() const override;

private:
    ShaderCompileParameters m_shader;
};

class GraphicsPsoDesc : public PsoDesc
{
public:
    GraphicsPsoDesc(PsoDesc desc) : PsoDesc(desc)
    {
    }

    GraphicsPsoDesc(std::string name) : PsoDesc(PsoType::Graphics, name)
    {
    }

    PsoHash GenerateHash() const override;
    std::optional<std::vector<ShaderHash>> BuildShaders(wil::com_ptr<DxDevice> device, std::shared_ptr<ShaderStorage> shader_storage) override;

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
    //std::shared_ptr<D3D12_INPUT_ELEMENT_DESC> m_input_layout;
};

class PsoStorage
{
public:
    bool Initialize();

    // old pso id if we recompile PSO and want to keep id
    std::optional<PsoId> BuildPso(wil::com_ptr<DxDevice> device, PsoDesc* pso_desc, PsoId old_pso_id = -1);

    std::optional<Pso> GetPso(PsoId pso_id);

    // Hide after raytracing manual shader compilation is fixed
    std::shared_ptr<ShaderStorage> GetShaderStorage()
    {
        return m_shader_storage;
    }

    void Tick(wil::com_ptr<DxDevice> device)
    {
        const auto modified_shaders = m_shader_storage->VerifyAnyShaderChanged();

        if (!modified_shaders.empty())
        {
            LogInfo << "Modified shaders found, recompiling...\n";

            for (const auto& shader : modified_shaders)
            {
                LogInfo << "Shader modified: " << WStringToString(shader.compile_parameters.shader_path) << " recompiling next PSOs\n";

                const auto& psos_to_recompile = m_shader_to_pso[shader.shader_hash];

                for (const auto& pso_id : psos_to_recompile)
                {
                    const auto pso_hash = m_pso_virtual_map[pso_id];
                    const auto pso_obj = m_pso_storage[pso_hash];
                    LogInfo << "-> [" << std::to_string(pso_id) << "] : " << pso_obj.pso_desc.GetName() << "\n";

                    // Remove old shader
                    // Remove old pso

                    //if(shader.compile_parameters.type == ShaderType::Vertex)
                    //{
                    //}
                    //else if (shader.compile_parameters.type == ShaderType::Pixel)
                    //{
                    //
                    //}
                    //else if (shader.compile_parameters.type == ShaderType::Compute)
                    //{
                    //
                    //}

                    m_shader_storage->RemoveShader(shader.shader_hash);

                    if(pso_obj.pso_desc.type == PsoType::Graphics)
                    {
                        GraphicsPsoDesc pso_desc(pso_obj.pso_desc);
                        pso_desc.AddShader(shader.compile_parameters);
                        BuildPso(device, &pso_desc, pso_id);
                    }
                    else if (pso_obj.pso_desc.type == PsoType::Compute)
                    {
                        ComputePsoDesc pso_desc(pso_obj.pso_desc);
                        pso_desc.AddShader(shader.compile_parameters);
                        BuildPso(device, &pso_desc, pso_id);
                    }
                }
            }
        }
    }

private:
    std::unordered_map<PsoId, PsoHash> m_pso_virtual_map;
    std::unordered_map<ShaderHash, std::unordered_set<PsoId>> m_shader_to_pso;
    std::unordered_map<PsoHash, Pso> m_pso_storage;
    std::shared_ptr<ShaderStorage> m_shader_storage;
};

} // namespace ysn

module :private;

namespace ysn
{

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
    m_compute_pso_desc.pRootSignature = root_signature;
    //AssertMsg(m_compute_pso_desc.pRootSignature != nullptr, "Root signature can't be nullptr");
}

PsoHash ComputePsoDesc::GenerateHash() const
{
    PsoHash pso_hash = HashState(&m_compute_pso_desc);

    return pso_hash;
}

std::optional<std::vector<ShaderHash>> ComputePsoDesc::BuildShaders(wil::com_ptr<DxDevice> device, std::shared_ptr<ShaderStorage> shader_storage)
{
    std::vector<ShaderHash> result;

    const auto& compiled_shader_hash = shader_storage->CompileShader(m_shader);

    if (!compiled_shader_hash.has_value())
    {
        LogError << "Can't compile shader\n";
        return std::nullopt;
    }

    wil::com_ptr<IDxcBlob> blob = shader_storage->GetShader(*compiled_shader_hash).value();
    const auto bytecode = CD3DX12_SHADER_BYTECODE(const_cast<void*>(blob->GetBufferPointer()), blob->GetBufferSize());

    m_compute_pso_desc.CS = bytecode;

    result.emplace_back(*compiled_shader_hash);

    return result;
}

ID3D12RootSignature* ComputePsoDesc::GetRootSignature() const
{
    return m_compute_pso_desc.pRootSignature;
}

PsoHash GraphicsPsoDesc::GenerateHash() const
{
    auto pso_desc_copy = m_graphics_pso_desc;

    pso_desc_copy.InputLayout.pInputElementDescs = nullptr;

    PsoHash pso_hash = HashState(&pso_desc_copy);
    // TODO(hotreload): Finish this shit
    //pso_hash = HashState(m_input_layout.get(), m_graphics_pso_desc.InputLayout.NumElements, pso_hash);

    // m_pso_desc.InputLayout.pInputElementDescs = m_input_layout.get();

    return pso_hash;
}

std::optional<std::vector<ShaderHash>> GraphicsPsoDesc::BuildShaders(wil::com_ptr<DxDevice> device, std::shared_ptr<ShaderStorage> shader_storage)
{
    std::vector<ShaderHash> result;

    for (const auto& shader : m_shaders_to_compile)
    {
        const auto& compiled_shader_hash = shader_storage->CompileShader(shader);

        if (!compiled_shader_hash.has_value())
        {
            LogError << "Can't compile shader\n";
            return std::nullopt;
        }

        wil::com_ptr<IDxcBlob> blob = shader_storage->GetShader(*compiled_shader_hash).value();
        const auto bytecode = CD3DX12_SHADER_BYTECODE(const_cast<void*>(blob->GetBufferPointer()), blob->GetBufferSize());

        switch (shader.type)
        {
        case ShaderType::Vertex:
            m_graphics_pso_desc.VS = bytecode;
            break;
        case ShaderType::Pixel:
            m_graphics_pso_desc.PS = bytecode;
            break;
            // TODO: finish and sanitize
        }

        result.emplace_back(*compiled_shader_hash);
    }

    return result;
}

const D3D12_GRAPHICS_PIPELINE_STATE_DESC& GraphicsPsoDesc::GetDesc() const
{
    return m_graphics_pso_desc;
}

const D3D12_COMPUTE_PIPELINE_STATE_DESC& ComputePsoDesc::GetDesc() const
{
    return m_compute_pso_desc;
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
    return m_graphics_pso_desc.pRootSignature;
}

void GraphicsPsoDesc::SetRootSignature(ID3D12RootSignature* root_signature)
{
    m_graphics_pso_desc.pRootSignature = root_signature;
    AssertMsg(m_graphics_pso_desc.pRootSignature != nullptr, "Root signature can't be nullptr");
}

void GraphicsPsoDesc::SetBlendState(const D3D12_BLEND_DESC& blend_desc)
{
    m_graphics_pso_desc.BlendState = blend_desc;
}

void GraphicsPsoDesc::SetRasterizerState(const D3D12_RASTERIZER_DESC& rasterizer_desc)
{
    m_graphics_pso_desc.RasterizerState = rasterizer_desc;
}

void GraphicsPsoDesc::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& depth_stencil_desc)
{
    m_graphics_pso_desc.DepthStencilState = depth_stencil_desc;
}

void GraphicsPsoDesc::SetSampleMask(UINT sample_mask)
{
    m_graphics_pso_desc.SampleMask = sample_mask;
}

void GraphicsPsoDesc::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type)
{
    AssertMsg(topology_type != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED, "Can't draw with undefined topology");
    m_graphics_pso_desc.PrimitiveTopologyType = topology_type;
}

void GraphicsPsoDesc::SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ib_props)
{
    m_graphics_pso_desc.IBStripCutValue = ib_props;
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
        m_graphics_pso_desc.RTVFormats[i] = RTVFormats[i];
    }
    // for (UINT i = NumRTVs; i < m_graphics_pso_desc.NumRenderTargets; ++i)
    //	m_graphics_pso_desc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
    m_graphics_pso_desc.NumRenderTargets = NumRTVs;
    m_graphics_pso_desc.DSVFormat = DSVFormat;
    m_graphics_pso_desc.SampleDesc.Count = MsaaCount;
    m_graphics_pso_desc.SampleDesc.Quality = MsaaQuality;
}

void GraphicsPsoDesc::SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs)
{
    m_graphics_pso_desc.InputLayout.NumElements = NumElements;

    if (NumElements > 0)
    {
        D3D12_INPUT_ELEMENT_DESC* new_elements = (D3D12_INPUT_ELEMENT_DESC*)malloc(sizeof(D3D12_INPUT_ELEMENT_DESC) * NumElements);
        memcpy(new_elements, pInputElementDescs, NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
        //m_input_layout.reset((D3D12_INPUT_ELEMENT_DESC*)new_elements);

        m_graphics_pso_desc.InputLayout.pInputElementDescs = new_elements;
    }
    else
    {
        //m_input_layout = nullptr;
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

std::optional<PsoId> PsoStorage::BuildPso(wil::com_ptr<DxDevice> device, PsoDesc* pso_desc, PsoId old_pso_id)
{
    // TODO: Check result
    const auto shaders_hashes = pso_desc->BuildShaders(device, m_shader_storage);

    if (!shaders_hashes.has_value())
        return std::nullopt;

    const auto pso_hash = pso_desc->GenerateHash();
    bool should_create_new = !m_pso_storage.contains(pso_hash);

    if (should_create_new)
    {
        // YSN_ASSERT(pso_ref->DepthStencilState.DepthEnable != (m_pso_desc.DSVFormat == DXGI_FORMAT_UNKNOWN));

        const auto pso_id = old_pso_id == -1 ? GeneratePsoId() : old_pso_id;

        Pso new_pso;

        // Associate PSO and shaders for hot reloading
        for (const auto& shader_hash : *shaders_hashes)
        {
            m_shader_to_pso[shader_hash].insert(pso_id);
        }

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
        new_pso.pso_desc = *pso_desc;

#ifndef YSN_RELEASE
        new_pso.pso->SetName(pso_desc->GetNameWString().c_str());
#endif
        m_pso_virtual_map[pso_id] = pso_hash;
        m_pso_storage[pso_hash] = new_pso;
    }
    else
    {
        LogInfo << "Cache hit for pso: " << pso_desc->GetName() << " [" << std::to_string(m_pso_storage[pso_hash].pso_id) << "] \n";
    }

    return m_pso_storage[pso_hash].pso_id;
}

std::optional<Pso> PsoStorage::GetPso(PsoId pso_id)
{
    const bool pso_hash_exist = m_pso_virtual_map.contains(pso_id);

    if (!pso_hash_exist)
        return std::nullopt;

    const PsoHash pso_hash = m_pso_virtual_map[pso_id];
    const bool pso_exist = m_pso_storage.contains(pso_hash);

    if (!pso_exist)
        return std::nullopt;

    return m_pso_storage[pso_hash];
}
} // namespace ysn