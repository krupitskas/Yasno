module;

#include <wil/com.h>
#include <d3dx12.h>

export module graphics.techniques.skybox_pass;

import std;
import graphics.primitive;
import renderer.dxrenderer;
import renderer.descriptor_heap;
import renderer.gpu_texture;
import renderer.gpu_pixel_buffer;
import renderer.command_queue;
import system.string_helpers;
import system.filesystem;
import system.application;
import system.logger;

export namespace ysn
{
struct SkyboxPassParameters
{
    std::shared_ptr<CommandQueue> command_queue = nullptr;
    std::shared_ptr<CbvSrvUavDescriptorHeap> cbv_srv_uav_heap = nullptr;
    wil::com_ptr<ID3D12Resource> scene_color_buffer = nullptr;
    DescriptorHandle hdr_rtv_descriptor_handle;
    DescriptorHandle dsv_descriptor_handle;
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissors_rect;
    GpuPixelBuffer3D cubemap_texture;
    wil::com_ptr<ID3D12Resource> camera_gpu_buffer;

    // TODO: debug renderer test below
    DescriptorHandle debug_counter_buffer_uav;
    DescriptorHandle debug_vertices_buffer_uav;
};

class SkyboxPass
{
    enum ShaderInputParameters
    {
        CameraBuffer,
        InputTexture,
        DebugVertices,
        DebugVerticesCount,
        ParametersCount
    };

public:
    bool Initialize();
    bool RenderSkybox(SkyboxPassParameters* parameters);

private:
    void UpdateParameters();

    MeshPrimitive cube;

    wil::com_ptr<ID3D12RootSignature> m_root_signature;
    wil::com_ptr<ID3D12PipelineState> m_pipeline_state;
};
} // namespace ysn

module :private;

namespace ysn
{
bool SkyboxPass::Initialize()
{
    const auto box_result = ConstructBox();

    if (!box_result.has_value())
    {
        return false;
    }

    cube = box_result.value();
    // CD3DX12_PIPELINE_STATE_STREAM_VIEW_INSTANCING ???
    // A helper structure used to wrap a CD3DX12_VIEW_INSTANCING_DESC structure. Allows shaders to render to multiple views with a single draw call; useful for stereo vision or cubemap generation.

    CD3DX12_DESCRIPTOR_RANGE cubemap_srv(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0);

    CD3DX12_DESCRIPTOR_RANGE debug_vertices_uav(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 126, 0, 0);
    CD3DX12_DESCRIPTOR_RANGE debug_counter_uav(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 127, 0, 0);

    CD3DX12_ROOT_PARAMETER root_parameters[ShaderInputParameters::ParametersCount] = {};
    root_parameters[ShaderInputParameters::CameraBuffer].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    root_parameters[ShaderInputParameters::InputTexture].InitAsDescriptorTable(1, &cubemap_srv, D3D12_SHADER_VISIBILITY_PIXEL);
    root_parameters[ShaderInputParameters::DebugVertices].InitAsDescriptorTable(1, &debug_vertices_uav, D3D12_SHADER_VISIBILITY_ALL);
    root_parameters[ShaderInputParameters::DebugVerticesCount].InitAsDescriptorTable(1, &debug_counter_uav, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_STATIC_SAMPLER_DESC static_sampler(
        0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
    root_signature_desc.NumParameters = ShaderInputParameters::ParametersCount;
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

    ShaderCompileParameters vs_parameters(ShaderType::Vertex, VfsPath(L"shaders/skybox.vs.hlsl"));
    const auto vs_shader_result = Application::Get().GetRenderer()->GetShaderStorage()->CompileShader(vs_parameters);

    if (!vs_shader_result.has_value())
    {
        LogError << "Can't compile SkyBox vs shader\n";
        return false;
    }

    ShaderCompileParameters ps_parameters(ShaderType::Pixel, ysn::VfsPath(L"shaders/skybox.ps.hlsl"));
    const auto ps_shader_result = Application::Get().GetRenderer()->GetShaderStorage()->CompileShader(ps_parameters);

    if (!ps_shader_result.has_value())
    {
        LogError << "Can't compile Skybox ps shader\n";
        return false;
    }

    D3D12_RASTERIZER_DESC rasterizer_desc = {};
    rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer_desc.CullMode = D3D12_CULL_MODE_NONE; // TODO: Should be back?

    D3D12_BLEND_DESC blend_desc = {};
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_desc = {};
    pipeline_state_desc.pRootSignature = m_root_signature.get();
    pipeline_state_desc.VS = {vs_shader_result.value()->GetBufferPointer(), vs_shader_result.value()->GetBufferSize()};
    pipeline_state_desc.PS = {ps_shader_result.value()->GetBufferPointer(), ps_shader_result.value()->GetBufferSize()};
    pipeline_state_desc.RasterizerState = rasterizer_desc;
    pipeline_state_desc.BlendState = blend_desc;
    pipeline_state_desc.DepthStencilState.DepthEnable = true;
    pipeline_state_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pipeline_state_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pipeline_state_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // TODO: provide
    pipeline_state_desc.SampleMask = UINT_MAX;
    pipeline_state_desc.InputLayout = {cube.input_layout_desc.data(), static_cast<UINT>(cube.input_layout_desc.size())};
    pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline_state_desc.NumRenderTargets = 1;
    pipeline_state_desc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT; // TODO: Get it from rendertarget
    pipeline_state_desc.SampleDesc = {1, 0};

    if (auto hr_result =
            Application::Get().GetDevice()->CreateGraphicsPipelineState(&pipeline_state_desc, IID_PPV_ARGS(&m_pipeline_state));
        hr_result != S_OK)
    {
        LogError << "Can't create SkyBox pipeline state object :" << ConvertHrToString(hr_result) << "\n";
        return false;
    }

    return true;
}

bool SkyboxPass::RenderSkybox(SkyboxPassParameters* parameters)
{
    const auto command_list_result = parameters->command_queue->GetCommandList("Skybox");

    if(!command_list_result.has_value())
        return false;

    GraphicsCommandList command_list = command_list_result.value();

    ID3D12DescriptorHeap* ppHeaps[] = {parameters->cbv_srv_uav_heap->GetHeapPtr()};
    command_list.list->RSSetViewports(1, &parameters->viewport);
    command_list.list->RSSetScissorRects(1, &parameters->scissors_rect);
    command_list.list->OMSetRenderTargets(1, &parameters->hdr_rtv_descriptor_handle.cpu, FALSE, &parameters->dsv_descriptor_handle.cpu);
    command_list.list->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    command_list.list->SetPipelineState(m_pipeline_state.get());
    command_list.list->SetGraphicsRootSignature(m_root_signature.get());
    command_list.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    command_list.list->IASetVertexBuffers(0, 1, &cube.vertex_buffer_view);

    command_list.list->SetGraphicsRootConstantBufferView(ShaderInputParameters::CameraBuffer, parameters->camera_gpu_buffer->GetGPUVirtualAddress());
    command_list.list->SetGraphicsRootDescriptorTable(ShaderInputParameters::InputTexture, parameters->cubemap_texture.srv.gpu);
    command_list.list->SetGraphicsRootDescriptorTable(ShaderInputParameters::DebugVertices, parameters->debug_vertices_buffer_uav.gpu);
    command_list.list->SetGraphicsRootDescriptorTable(ShaderInputParameters::DebugVerticesCount, parameters->debug_counter_buffer_uav.gpu);

    command_list.list->IASetIndexBuffer(&cube.index_buffer_view);
    command_list.list->DrawIndexedInstanced(cube.index_count, 1, 0, 0, 0);

    parameters->command_queue->ExecuteCommandList(command_list);

    return true;
}
} // namespace ysn
