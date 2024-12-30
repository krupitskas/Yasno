module;

#include <d3d12.h>
#include <d3dx12.h>
#include <wil/com.h>
#include <DirectXMath.h>

export module graphics.techniques.convert_to_cubemap;

import system.string_helpers;
import system.filesystem;
import system.application;
import system.logger;
import renderer.dxrenderer;
import renderer.gpu_texture;
import renderer.gpu_pixel_buffer;
import renderer.vertex_storage;
import graphics.primitive;

export namespace ysn
{
struct ConvertToCubemapParameters
{
    wil::com_ptr<ID3D12Resource> camera_buffer;
    GpuTexture source_texture;
    GpuPixelBuffer3D target_cubemap;
};

class ConvertToCubemap
{
public:
    ConvertToCubemap() = default;
    ~ConvertToCubemap();
    bool Initialize();
    bool Render(ConvertToCubemapParameters& parameters);

private:
    ID3D12RootSignature* m_root_signature = nullptr;
    ID3D12PipelineState* m_pipeline_state = nullptr;

    MeshPrimitive cube;

    DirectX::XMMATRIX m_projection;
    DirectX::XMMATRIX m_views[6];

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissors_rect;
};
} // namespace ysn

module :private;

namespace ysn
{

using namespace DirectX;

enum ShaderParameters
{
    ViewProjectionMatrix,
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
    const auto box_result = ConstructBox();

    if (!box_result.has_value())
    {
        return false;
    }

    cube = box_result.value();

    m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(512), static_cast<float>(512));
    m_scissors_rect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);

    CD3DX12_DESCRIPTOR_RANGE source_texture_srv(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0);

    CD3DX12_ROOT_PARAMETER root_parameters[ShaderParameters::ParametersCount] = {};
    root_parameters[ShaderParameters::ViewProjectionMatrix].InitAsConstants(sizeof(XMMATRIX) / sizeof(float), 0);
    root_parameters[ShaderParameters::InputTexture].InitAsDescriptorTable(1, &source_texture_srv);

    CD3DX12_STATIC_SAMPLER_DESC static_sampler(
        0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

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

    ShaderCompileParameters<VertexShader> vs_parameters(VfsPath(L"shaders/convert_equirectangular_map.vs.hlsl"));
    const auto vs_shader_result = Application::Get().GetRenderer()->GetShaderStorage()->CompileShader(vs_parameters);

    if (!vs_shader_result.has_value())
    {
        LogError << "Can't compile ConvertToCubemap vs shader\n";
        return false;
    }

    ShaderCompileParameters<PixelShader> ps_parameters(VfsPath(L"shaders/convert_equirectangular_map.ps.hlsl"));
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

    D3D12_BLEND_DESC blend_desc = {};
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc = {};
    pipeline_state_desc.pRootSignature = m_root_signature;
    pipeline_state_desc.VS = {vs_shader_result.value()->GetBufferPointer(), vs_shader_result.value()->GetBufferSize()};
    pipeline_state_desc.PS = {ps_shader_result.value()->GetBufferPointer(), ps_shader_result.value()->GetBufferSize()};
    pipeline_state_desc.RasterizerState = rasterizer_desc;
    pipeline_state_desc.DepthStencilState.DepthEnable = false;
    pipeline_state_desc.InputLayout = {input_element_desc.data(), static_cast<UINT>(input_element_desc.size())};
    pipeline_state_desc.BlendState = blend_desc;
    pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline_state_desc.NumRenderTargets = 1;
    pipeline_state_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT; // TODO: Get it from rendertarget
    pipeline_state_desc.SampleDesc = {1, 0};
    pipeline_state_desc.SampleMask = UINT_MAX;

    if (auto hr_result =
            Application::Get().GetDevice()->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&m_pipeline_state));
        hr_result != S_OK)
    {
        LogError << "Can't create ConvertToCubemap pipeline state object :" << ConvertHrToString(hr_result) << "\n";
        return false;
    }

    const XMVECTOR position = XMVectorSet(0.0, 0.0, 0.0, 1.0);

    // TODO: Sus! 0 and 1 are reversed.
    m_views[0] = XMMatrixLookAtRH(position, XMVectorSet(-1.0, 0.0, 0.0, 0.0), XMVectorSet(0.0, 1.0, 0.0, 0.0));
    m_views[1] = XMMatrixLookAtRH(position, XMVectorSet(1.0, 0.0, 0.0, 0.0), XMVectorSet(0.0, 1.0, 0.0, 0.0));
    m_views[2] = XMMatrixLookAtRH(position, XMVectorSet(0.0, 1.0, 0.0, 0.0), XMVectorSet(0.0, 0.0, -1.0, 0.0));
    m_views[3] = XMMatrixLookAtRH(position, XMVectorSet(0.0, -1.0, 0.0, 0.0), XMVectorSet(0.0, 0.0, 1.0, 0.0));
    m_views[4] = XMMatrixLookAtRH(position, XMVectorSet(0.0, 0.0, 1.0, 0.0), XMVectorSet(0.0, 1.0, 0.0, 0.0));
    m_views[5] = XMMatrixLookAtRH(position, XMVectorSet(0.0, 0.0, -1.0, 0.0), XMVectorSet(0.0, 1.0, 0.0, 0.0));

    m_projection = XMMatrixPerspectiveFovRH(XMConvertToRadians(90.f), 1.0f, 0.1f, 10.f);

    return true;
}

bool ConvertToCubemap::Render(ConvertToCubemapParameters& parameters)
{
    const auto renderer = Application::Get().GetRenderer();

    const auto command_list_result = renderer->GetDirectQueue()->GetCommandList("Convert Cubemap");

    if (!command_list_result.has_value())
        return false;

    GraphicsCommandList command_list = command_list_result.value();

    CD3DX12_RESOURCE_BARRIER barrier_before = CD3DX12_RESOURCE_BARRIER::Transition(
        parameters.target_cubemap.resource.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    command_list.list->ResourceBarrier(1, &barrier_before);

    ID3D12DescriptorHeap* ppHeaps[] = {renderer->GetCbvSrvUavDescriptorHeap()->GetHeapPtr()};
    command_list.list->RSSetViewports(1, &m_viewport);
    command_list.list->RSSetScissorRects(1, &m_scissors_rect);
    command_list.list->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    command_list.list->SetPipelineState(m_pipeline_state);
    command_list.list->SetGraphicsRootSignature(m_root_signature);
    command_list.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    command_list.list->IASetVertexBuffers(0, 1, &cube.vertex_buffer_view);
    command_list.list->IASetIndexBuffer(&cube.index_buffer_view);

    command_list.list->SetGraphicsRootDescriptorTable(1, parameters.source_texture.descriptor_handle.gpu);

    for (int i = 0; i < 6; i++)
    {
        DirectX::XMMATRIX view_projection = DirectX::XMMatrixIdentity();
        view_projection = XMMatrixMultiply(DirectX::XMMatrixIdentity(), m_views[i]);
        view_projection = XMMatrixMultiply(view_projection, m_projection);
        float data[16];
        XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(data), view_projection);

        command_list.list->SetGraphicsRoot32BitConstants(ShaderParameters::ViewProjectionMatrix, 16, data, 0);
        command_list.list->OMSetRenderTargets(1, &parameters.target_cubemap.rtv[i].cpu, FALSE, nullptr);
        command_list.list->DrawIndexedInstanced(cube.index_count, 1, 0, 0, 0);
    }

    CD3DX12_RESOURCE_BARRIER barrier_after = CD3DX12_RESOURCE_BARRIER::Transition(
        parameters.target_cubemap.resource.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    command_list.list->ResourceBarrier(1, &barrier_after);

    renderer->GetDirectQueue()->ExecuteCommandList(command_list);

    return true;
}

} // namespace ysn
