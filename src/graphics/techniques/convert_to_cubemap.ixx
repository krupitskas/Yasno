module;

#include <d3d12.h>
#include <d3dx12.h>
// #include <wil/com.h>

export module graphics.techniques.convert_to_cubemap;

import system.string_helpers;
import system.filesystem;
import system.application;
import system.logger;
import renderer.dxrenderer;

export namespace ysn
{
class ConvertToCubemap
{
public:
    ConvertToCubemap() = default;
    ~ConvertToCubemap();
    bool Initialize();

private:
    ID3D12RootSignature* m_root_signature = nullptr;
    ID3D12PipelineState* m_pipeline_state = nullptr;
};
} // namespace ysn

module :private;

namespace ysn
{
enum ShaderParameters
{
    ViewProjection,
    InputTexture,
    ParametersCount
};

ConvertToCubemap::~ConvertToCubemap()
{
    if (m_root_signature != nullptr)
    {
        m_root_signature->Release();
        m_root_signature = nullptr;
    }

    if (m_pipeline_state != nullptr)
    {
        m_pipeline_state->Release();
        m_pipeline_state = nullptr;
    }
}

bool ConvertToCubemap::Initialize()
{
    CD3DX12_DESCRIPTOR_RANGE cubemap_srv(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0);

    CD3DX12_ROOT_PARAMETER root_parameters[ShaderParameters::ParametersCount] = {};
    root_parameters[ShaderParameters::ViewProjection].InitAsConstantBufferView(0);
    root_parameters[ShaderParameters::InputTexture].InitAsDescriptorTable(1, &cubemap_srv);

    CD3DX12_STATIC_SAMPLER_DESC static_sampler(
        0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
    root_signature_desc.NumParameters = ShaderParameters::ParametersCount;
    root_signature_desc.pParameters = &root_parameters[0];
    root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    root_signature_desc.pStaticSamplers = &static_sampler;
    root_signature_desc.NumStaticSamplers = 1;

    bool result = Application::Get().GetRenderer()->CreateRootSignature(&root_signature_desc, &m_root_signature);

    if (!result)
    {
        LogError << "Can't create ConvertToCubemap root signature\n";
        return false;
    }

    ysn::ShaderCompileParameters vs_parameters;
    vs_parameters.shader_type = ysn::ShaderType::Vertex;
    vs_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"shaders/convert_equirectangular_map.vs.hlsl");

    const auto vs_shader_result = Application::Get().GetRenderer()->GetShaderStorage()->CompileShader(vs_parameters);

    if (!vs_shader_result.has_value())
    {
        LogError << "Can't compile ConvertToCubemap vs shader\n";
        return false;
    }

    ysn::ShaderCompileParameters ps_parameters;
    ps_parameters.shader_type = ysn::ShaderType::Pixel;
    ps_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"shaders/convert_equirectangular_map.ps.hlsl");

    const auto ps_shader_result = Application::Get().GetRenderer()->GetShaderStorage()->CompileShader(ps_parameters);

    if (!ps_shader_result.has_value())
    {
        LogError << "Can't compile ConvertToCubemap ps shader\n";
        return false;
    }

    std::vector<D3D12_INPUT_ELEMENT_DESC> input_element_desc;

    D3D12_INPUT_ELEMENT_DESC position_desc = {};
    position_desc.SemanticName = "POSITION";
    position_desc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
    // position_desc.SemanticIndex = 0;
    position_desc.InputSlot = 0;
    position_desc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    position_desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    D3D12_INPUT_ELEMENT_DESC texture_desc = {};
    texture_desc.SemanticName = "TEXCOORD";
    texture_desc.Format = DXGI_FORMAT_R32G32_FLOAT;
    // texture_desc.SemanticIndex = 0;
    texture_desc.InputSlot = 0;
    texture_desc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    texture_desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    input_element_desc.push_back(position_desc);
    input_element_desc.push_back(texture_desc);

    D3D12_RASTERIZER_DESC rasterizer_desc = {};
    rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer_desc.CullMode = D3D12_CULL_MODE_NONE; // TODO: Should be back?

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc = {};
    pipeline_state_desc.pRootSignature = m_root_signature;
    pipeline_state_desc.VS = {vs_shader_result.value()->GetBufferPointer(), vs_shader_result.value()->GetBufferSize()};
    pipeline_state_desc.PS = {ps_shader_result.value()->GetBufferPointer(), ps_shader_result.value()->GetBufferSize()};
    pipeline_state_desc.RasterizerState = rasterizer_desc;
    pipeline_state_desc.DepthStencilState.DepthEnable = false;
    pipeline_state_desc.InputLayout = {input_element_desc.data(), static_cast<UINT>(input_element_desc.size())};
    pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline_state_desc.NumRenderTargets = 1;
    pipeline_state_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT; // TODO: Get it from rendertarget
    pipeline_state_desc.SampleDesc = {1, 0};
    pipeline_state_desc.SampleMask = 1;

    if (auto hr_result =
            Application::Get().GetDevice()->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&m_pipeline_state));
        hr_result != S_OK)
    {
        LogError << "Can't create ConvertToCubemap pipeline state object :" << ConvertHrToString(hr_result) << "\n";
        return false;
    }

    // TODO: Generate 6 RTV for each face of cubemap

    // Projection
    // All views
    // Camera CBV buffer

    // glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    // glm::mat4 captureViews[] =
    // {
    // 	glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    // 	glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    // 	glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
    // 	glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
    // 	glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    // 	glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    //  };

    return true;
}
} // namespace ysn
