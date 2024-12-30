module;

#include <d3d12.h>
#include <d3dx12.h>
#include <wil/com.h>

#include <shader_structs.h>

export module graphics.techniques.forward_pass;

import graphics.primitive;
import graphics.render_scene;
import graphics.material;
import graphics.techniques.shadow_map_pass;
import renderer.pso;
import renderer.command_queue;
import renderer.dxrenderer;
import renderer.descriptor_heap;
import renderer.gpu_texture;
import renderer.gpu_buffer;
import system.filesystem;
import system.application;
import system.logger;
import system.asserts;

namespace ysn
{
// Data structure to match the command signature used for ExecuteIndirect.
struct IndirectCommand
{
    D3D12_GPU_VIRTUAL_ADDRESS camera_parameters_cbv;
    D3D12_GPU_VIRTUAL_ADDRESS scene_parameters_cbv;
    D3D12_GPU_VIRTUAL_ADDRESS per_instance_data_cbv;
    D3D12_DRAW_INDEXED_ARGUMENTS draw_arguments;
};
}

export namespace ysn
{
struct ForwardPassRenderParameters
{
    std::shared_ptr<CommandQueue> command_queue = nullptr;
    std::shared_ptr<CbvSrvUavDescriptorHeap> cbv_srv_uav_heap = nullptr;
    wil::com_ptr<ID3D12Resource> scene_color_buffer = nullptr;
    DescriptorHandle hdr_rtv_descriptor_handle;
    DescriptorHandle dsv_descriptor_handle;
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissors_rect;
    wil::com_ptr<ID3D12Resource> camera_gpu_buffer;
    wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer;
    D3D12_CPU_DESCRIPTOR_HANDLE backbuffer_handle;
    wil::com_ptr<ID3D12Resource> current_back_buffer;
    ShadowMapBuffer shadow_map_buffer;
};


enum class IndirectRootParameters : uint8_t
{
    CameraParametersSrv,
    SceneParametersSrv,
    PerInstanceDataSrv,
    Count
};

struct ForwardPass
{
    bool Initialize(
        const RenderScene& render_scene,
        wil::com_ptr<ID3D12Resource> camera_gpu_buffer,
        wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer,
        const GpuBuffer& instance_buffer,
        wil::com_ptr<ID3D12GraphicsCommandList4> cmd_list);
    bool InitializeIndirectPipeline(
        const RenderScene& render_scene,
        wil::com_ptr<ID3D12Resource> camera_gpu_buffer,
        wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer,
        const GpuBuffer& instance_buffer,
        wil::com_ptr<ID3D12GraphicsCommandList4> cmd_list);
    bool CompilePrimitivePso(ysn::Primitive& primitive, std::vector<Material> materials);
    bool Render(const RenderScene& render_scene, const ForwardPassRenderParameters& render_parameters);
    bool RenderIndirect(const RenderScene& render_scene, const ForwardPassRenderParameters& render_parameters);

    // Indirect data
    wil::com_ptr<ID3D12RootSignature> m_indirect_root_signature;
    std::vector<IndirectCommand> m_indirect_commands;
    wil::com_ptr<ID3D12CommandSignature> m_command_signature;
    GpuBuffer m_command_buffer;
    PsoId indirect_pso_id = 0;
    uint32_t m_command_buffer_size = 0; // PrimitiveCount * sizeof(IndirectCommand)
};
} // namespace ysn

module :private;

namespace ysn
{
bool ForwardPass::CompilePrimitivePso(ysn::Primitive& primitive, std::vector<Material> materials)
{
    std::shared_ptr<DxRenderer> renderer = Application::Get().GetRenderer();

    GraphicsPsoDesc new_pso_desc("Primitive PSO");

    {
        bool result = false;

        D3D12_DESCRIPTOR_RANGE instance_data_input_range = {};
        instance_data_input_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        instance_data_input_range.NumDescriptors = 1;
        instance_data_input_range.BaseShaderRegister = 0;

        D3D12_ROOT_PARAMETER instance_buffer_parameter;
        instance_buffer_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        instance_buffer_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        instance_buffer_parameter.DescriptorTable.NumDescriptorRanges = 1;
        instance_buffer_parameter.DescriptorTable.pDescriptorRanges = &instance_data_input_range;

        D3D12_DESCRIPTOR_RANGE materials_input_range = {};
        materials_input_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        materials_input_range.NumDescriptors = 1;
        materials_input_range.BaseShaderRegister = 1;

        D3D12_ROOT_PARAMETER materials_buffer_parameter;
        materials_buffer_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        materials_buffer_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        materials_buffer_parameter.DescriptorTable.NumDescriptorRanges = 1;
        materials_buffer_parameter.DescriptorTable.pDescriptorRanges = &materials_input_range;

        D3D12_DESCRIPTOR_RANGE shadow_input_range = {};
        shadow_input_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        shadow_input_range.NumDescriptors = 1;
        shadow_input_range.BaseShaderRegister = 2;

        D3D12_ROOT_PARAMETER shadow_parameter;
        shadow_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        shadow_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        shadow_parameter.DescriptorTable.NumDescriptorRanges = 1;
        shadow_parameter.DescriptorTable.pDescriptorRanges = &shadow_input_range;

        D3D12_ROOT_PARAMETER rootParams[6] = {
            {D3D12_ROOT_PARAMETER_TYPE_CBV, {0, 0}, D3D12_SHADER_VISIBILITY_ALL}, // CameraParameters
            {D3D12_ROOT_PARAMETER_TYPE_CBV, {1, 0}, D3D12_SHADER_VISIBILITY_ALL}, // SceneParameters

            // InstanceID
            {.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
             .Constants = {.ShaderRegister = 2, .RegisterSpace = 0, .Num32BitValues = 1},
             .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL},

            instance_buffer_parameter,  // PerInstanceData
            materials_buffer_parameter, // Materials buffer
            shadow_parameter            // Shadow Map input
        };

        // TEMP
        rootParams[0].Descriptor.RegisterSpace = 0;
        rootParams[1].Descriptor.RegisterSpace = 0;

        // 0 ShadowSampler
        // 1 LinearSampler
        CD3DX12_STATIC_SAMPLER_DESC static_sampler[2];
        static_sampler[0] = CD3DX12_STATIC_SAMPLER_DESC(
            0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
        static_sampler[1] = CD3DX12_STATIC_SAMPLER_DESC(
            1,
            D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            0,
            0,
            D3D12_COMPARISON_FUNC_NONE);

        D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
        RootSignatureDesc.NumParameters = 6;
        RootSignatureDesc.pParameters = &rootParams[0];
        RootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                                  D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED // For bindless rendering
                                  | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
        RootSignatureDesc.pStaticSamplers = &static_sampler[0];
        RootSignatureDesc.NumStaticSamplers = 2;

        ID3D12RootSignature* root_signature = nullptr;

        result = renderer->CreateRootSignature(&RootSignatureDesc, &root_signature);

        if (!result)
        {
            LogError << "Can't create root signature for primitive\n";
            return false;
        }

        new_pso_desc.SetRootSignature(root_signature);
    }

    // Vertex shader
    {
        ShaderCompileParameters<VertexShader> vs_parameters(VfsPath(L"shaders/forward_pass.vs.hlsl"));
        const auto vs_shader_result = renderer->GetShaderStorage()->CompileShader(vs_parameters);

        if (!vs_shader_result.has_value())
        {
            LogError << "Can't compile GLTF forward pipeline vs shader\n";
            return false;
        }

        new_pso_desc.SetVertexShader(vs_shader_result.value()->GetBufferPointer(), vs_shader_result.value()->GetBufferSize());
    }

    // Pixel shader
    {
        ShaderCompileParameters<PixelShader> ps_parameters(VfsPath(L"shaders/forward_pass.ps.hlsl"));
        const auto ps_shader_result = renderer->GetShaderStorage()->CompileShader(ps_parameters);

        if (!ps_shader_result.has_value())
        {
            LogError << "Can't compile GLTF forward pipeline ps shader\n";
            return false;
        }

        new_pso_desc.SetPixelShader(ps_shader_result.value()->GetBufferPointer(), ps_shader_result.value()->GetBufferSize());
    }

    const auto& input_element_desc = renderer->GetInputElementsDesc();

    D3D12_DEPTH_STENCIL_DESC depth_stencil_desc = {};
    depth_stencil_desc.DepthEnable = true;
    depth_stencil_desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depth_stencil_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

    new_pso_desc.SetDepthStencilState(depth_stencil_desc);
    new_pso_desc.SetInputLayout(static_cast<UINT>(input_element_desc.size()), input_element_desc.data());
    new_pso_desc.SetSampleMask(UINT_MAX);

    const auto material = materials[primitive.material_id];

    new_pso_desc.SetRasterizerState(material.rasterizer_desc);
    new_pso_desc.SetBlendState(material.blend_desc);

    switch (primitive.topology)
    {
    case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
        new_pso_desc.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT);
        break;
    case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
    case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
        new_pso_desc.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
        break;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
        new_pso_desc.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        break;
    default:
        AssertMsg(false, "Unsupported primitive topology");
    }

    new_pso_desc.SetRenderTargetFormat(renderer->GetHdrFormat(), renderer->GetDepthBufferFormat());

    auto result_pso = renderer->CreatePso(new_pso_desc);

    if (!result_pso.has_value())
    {
        return false;
    }

    primitive.pso_id = *result_pso;

    return true;
}

bool ForwardPass::Render(const RenderScene& render_scene, const ForwardPassRenderParameters& render_parameters)
{
    auto renderer = Application::Get().GetRenderer();

    const auto cmd_list_res = render_parameters.command_queue->GetCommandList("Forward Pass");

    if (!cmd_list_res.has_value())
        return false;

    GraphicsCommandList command_list = cmd_list_res.value();

    ID3D12DescriptorHeap* pDescriptorHeaps[] = {
        render_parameters.cbv_srv_uav_heap->GetHeapPtr(),
    };
    command_list.list->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);
    command_list.list->RSSetViewports(1, &render_parameters.viewport);
    command_list.list->RSSetScissorRects(1, &render_parameters.scissors_rect);
    command_list.list->OMSetRenderTargets(
        1, &render_parameters.hdr_rtv_descriptor_handle.cpu, FALSE, &render_parameters.dsv_descriptor_handle.cpu);

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            render_parameters.shadow_map_buffer.buffer.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        command_list.list->ResourceBarrier(1, &barrier);
    }

    uint32_t instance_id = 0;

    for (auto& model : render_scene.models)
    {
        for (int mesh_id = 0; mesh_id < model.meshes.size(); mesh_id++)
        {
            const Mesh& mesh = model.meshes[mesh_id];

            for (int primitive_id = 0; primitive_id < mesh.primitives.size(); primitive_id++)
            {
                const Primitive& primitive = mesh.primitives[primitive_id];

                command_list.list->IASetVertexBuffers(0, 1, &primitive.vertex_buffer_view);

                // TODO: check for -1 as pso_id
                const std::optional<Pso> pso = renderer->GetPso(primitive.pso_id);

                if (pso.has_value())
                {
                    command_list.list->SetGraphicsRootSignature(pso.value().root_signature.get());
                    command_list.list->SetPipelineState(pso.value().pso.get());

                    command_list.list->IASetPrimitiveTopology(primitive.topology);

                    command_list.list->SetGraphicsRootConstantBufferView(0, render_parameters.camera_gpu_buffer->GetGPUVirtualAddress());
                    command_list.list->SetGraphicsRootConstantBufferView(
                        1, render_parameters.scene_parameters_gpu_buffer->GetGPUVirtualAddress());
                    command_list.list->SetGraphicsRoot32BitConstant(2, instance_id, 0); // InstanceID

                    command_list.list->SetGraphicsRootDescriptorTable(3, render_scene.instance_buffer_srv.gpu);
                    command_list.list->SetGraphicsRootDescriptorTable(4, render_scene.materials_buffer_srv.gpu);
                    command_list.list->SetGraphicsRootDescriptorTable(5, render_parameters.shadow_map_buffer.srv_handle.gpu);

                    if (primitive.index_count)
                    {
                        command_list.list->IASetIndexBuffer(&primitive.index_buffer_view);
                        command_list.list->DrawIndexedInstanced(primitive.index_count, 1, 0, 0, 0);
                    }
                    else
                    {
                        command_list.list->DrawInstanced(primitive.vertex_count, 1, 0, 0);
                    }
                }
                else
                {
                    LogInfo << "Can't render primitive because it haven't any PSO\n";
                }

                instance_id++;
            }
        }
    }

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            render_parameters.shadow_map_buffer.buffer.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        command_list.list->ResourceBarrier(1, &barrier);
    }

    render_parameters.command_queue->ExecuteCommandList(command_list);

    return true;
}

bool ForwardPass::Initialize(
    const RenderScene& render_scene,
    wil::com_ptr<ID3D12Resource> camera_gpu_buffer,
    wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer,
    const GpuBuffer& instance_buffer,
    wil::com_ptr<ID3D12GraphicsCommandList4> cmd_list)
{
    bool result = InitializeIndirectPipeline(render_scene, camera_gpu_buffer, scene_parameters_gpu_buffer, instance_buffer, cmd_list);

    if (!result)
    {
        LogError << "Forward pass can't initialize indirect pipeline\n";
        return false;
    }

    return true;
}

bool ForwardPass::InitializeIndirectPipeline(
    const RenderScene& render_scene,
    wil::com_ptr<ID3D12Resource> camera_gpu_buffer,
    wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer,
    const GpuBuffer& instance_buffer,
    wil::com_ptr<ID3D12GraphicsCommandList4> cmd_list)
{
    auto renderer = Application::Get().GetRenderer();

    // Create root signature
    {
        D3D12_ROOT_PARAMETER root_params[(uint8_t)IndirectRootParameters::Count] = {
            {D3D12_ROOT_PARAMETER_TYPE_CBV, {0, 0}, D3D12_SHADER_VISIBILITY_ALL}, // CameraParameters
            {D3D12_ROOT_PARAMETER_TYPE_CBV, {1, 0}, D3D12_SHADER_VISIBILITY_ALL}, // SceneParameters
            {D3D12_ROOT_PARAMETER_TYPE_CBV, {2, 0}, D3D12_SHADER_VISIBILITY_ALL}, // PerInstanceData
        };

        // TEMP
        root_params[0].Descriptor.RegisterSpace = 0;
        root_params[1].Descriptor.RegisterSpace = 0;
        root_params[2].Descriptor.RegisterSpace = 0;

        // 0 ShadowSampler
        // 1 LinearSampler
        CD3DX12_STATIC_SAMPLER_DESC static_sampler[2];
        static_sampler[0] = CD3DX12_STATIC_SAMPLER_DESC(
            0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
        static_sampler[1] = CD3DX12_STATIC_SAMPLER_DESC(
            1,
            D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            0,
            0,
            D3D12_COMPARISON_FUNC_NONE);

        D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
        RootSignatureDesc.NumParameters = (UINT)IndirectRootParameters::Count;
        RootSignatureDesc.pParameters = &root_params[0];
        RootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                                  D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED // For bindless rendering
                                  | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
        RootSignatureDesc.pStaticSamplers = &static_sampler[0];
        RootSignatureDesc.NumStaticSamplers = 2;

        bool result = renderer->CreateRootSignature(&RootSignatureDesc, &m_indirect_root_signature);

        if (!result)
        {
            LogError << "Can't create root signature for primitive\n";
            return false;
        }
    }

    // Create command signature
    D3D12_INDIRECT_ARGUMENT_DESC argument_desc[4] = {};
    argument_desc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
    argument_desc[0].ConstantBufferView.RootParameterIndex = 0;
    argument_desc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
    argument_desc[1].ConstantBufferView.RootParameterIndex = 1;
    argument_desc[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
    argument_desc[2].ConstantBufferView.RootParameterIndex = 2;
    argument_desc[3].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

    D3D12_COMMAND_SIGNATURE_DESC command_signature_desc = {};
    command_signature_desc.pArgumentDescs = argument_desc;
    command_signature_desc.NumArgumentDescs = _countof(argument_desc);
    command_signature_desc.ByteStride = sizeof(IndirectCommand);

    if (auto result = renderer->GetDevice()->CreateCommandSignature(
            &command_signature_desc, m_indirect_root_signature.get(), IID_PPV_ARGS(&m_command_signature));
        result != S_OK)
    {
        LogError << "Forward Pass can't create command signature\n";
        return false;
    }

    // Create PSO
    GraphicsPsoDesc pso_desc("Indirect Primitive PSO");

    pso_desc.SetRootSignature(m_indirect_root_signature.get());

    // Vertex shader
    {
        ShaderCompileParameters<VertexShader> vs_parameters(VfsPath(L"shaders/indirect_forward_pass.vs.hlsl"));
        const auto vs_shader_result = renderer->GetShaderStorage()->CompileShader(vs_parameters);

        if (!vs_shader_result.has_value())
        {
            LogError << "Can't compile indirect forward pipeline vs shader\n";
            return false;
        }

        pso_desc.SetVertexShader(vs_shader_result.value());
    }

    // Pixel shader
    {
        ysn::ShaderCompileParameters<PixelShader> ps_parameters(VfsPath(L"shaders/indirect_forward_pass.ps.hlsl"));
        const auto ps_shader_result = renderer->GetShaderStorage()->CompileShader(ps_parameters);

        if (!ps_shader_result.has_value())
        {
            LogError << "Can't compile indirect forward pipeline ps shader\n";
            return false;
        }

        pso_desc.SetPixelShader(ps_shader_result.value());
    }

    const auto& input_element_desc = renderer->GetInputElementsDesc();

    D3D12_DEPTH_STENCIL_DESC depth_stencil_desc = {};
    depth_stencil_desc.DepthEnable = true;
    depth_stencil_desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depth_stencil_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

    pso_desc.SetDepthStencilState(depth_stencil_desc);
    pso_desc.SetInputLayout(static_cast<UINT>(input_element_desc.size()), input_element_desc.data());
    pso_desc.SetSampleMask(UINT_MAX);

    // TODO: Should use GLTFs parameters
    D3D12_BLEND_DESC blend_desc = {};
    blend_desc.RenderTarget[0].BlendEnable = false;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterizer_desc = {};
    rasterizer_desc.CullMode = D3D12_CULL_MODE_FRONT;
    rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer_desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizer_desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizer_desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizer_desc.ForcedSampleCount = 0;
    rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    pso_desc.SetRasterizerState(rasterizer_desc);

    pso_desc.SetBlendState(blend_desc);
    pso_desc.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    pso_desc.SetRenderTargetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_D32_FLOAT);

    auto result_pso = renderer->CreatePso(pso_desc);

    if (!result_pso.has_value())
    {
        LogError << "Can't create indirect pso\n";
        return false;
    }

    indirect_pso_id = *result_pso;

    // Create commands
    m_indirect_commands.reserve(render_scene.primitives_count);

    D3D12_GPU_VIRTUAL_ADDRESS instance_data_buffer_gpu_address = instance_buffer.GetGPUVirtualAddress();
    UINT command_index = 0;

    // Fill commands
    for (auto& model : render_scene.models)
    {
        for (int mesh_id = 0; mesh_id < model.meshes.size(); mesh_id++)
        {
            const Mesh& mesh = model.meshes[mesh_id];

            for (int primitive_id = 0; primitive_id < mesh.primitives.size(); primitive_id++)
            {
                const Primitive& primitive = mesh.primitives[primitive_id];

                IndirectCommand command;

                command.camera_parameters_cbv = camera_gpu_buffer->GetGPUVirtualAddress();
                command.scene_parameters_cbv = scene_parameters_gpu_buffer->GetGPUVirtualAddress();
                command.per_instance_data_cbv = instance_data_buffer_gpu_address;

                command.draw_arguments.IndexCountPerInstance = primitive.index_count;
                command.draw_arguments.InstanceCount = 1;
                command.draw_arguments.StartIndexLocation = primitive.global_index_offset;
                command.draw_arguments.BaseVertexLocation = primitive.global_vertex_offset;
                command.draw_arguments.StartInstanceLocation = 0;

                m_indirect_commands.push_back(command);

                command_index++;
                instance_data_buffer_gpu_address += sizeof(PerInstanceData);
            }
        }
    }

    m_command_buffer_size = command_index * sizeof(IndirectCommand);

    GpuBufferCreateInfo create_info{
        .size = m_command_buffer_size, .heap_type = D3D12_HEAP_TYPE_DEFAULT, .state = D3D12_RESOURCE_STATE_COPY_DEST};

    const auto command_buffer_result = CreateGpuBuffer(create_info, "RenderInstance Buffer");

    if (!command_buffer_result.has_value())
    {
        LogError << "Can't create indirect command buffer\n";
        return false;
    }

    m_command_buffer = command_buffer_result.value();

    UploadToGpuBuffer(cmd_list, m_command_buffer, m_indirect_commands.data(), {}, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    return true;
}

bool ForwardPass::RenderIndirect(const RenderScene& render_scene, const ForwardPassRenderParameters& render_parameters)
{
    auto renderer = Application::Get().GetRenderer();

    const auto cmd_list_res = render_parameters.command_queue->GetCommandList("Indirect Forward Pass");

    if (!cmd_list_res.has_value())
        return false;

    GraphicsCommandList command_list = cmd_list_res.value();

    ID3D12DescriptorHeap* pDescriptorHeaps[] = {
        render_parameters.cbv_srv_uav_heap->GetHeapPtr(),
    };
    command_list.list->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);
    command_list.list->RSSetViewports(1, &render_parameters.viewport);
    command_list.list->RSSetScissorRects(1, &render_parameters.scissors_rect);
    command_list.list->OMSetRenderTargets(
        1, &render_parameters.hdr_rtv_descriptor_handle.cpu, FALSE, &render_parameters.dsv_descriptor_handle.cpu);

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            render_parameters.shadow_map_buffer.buffer.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        command_list.list->ResourceBarrier(1, &barrier);
    }

    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
    vertex_buffer_view.BufferLocation = render_scene.vertices_buffer.GetGPUVirtualAddress();
    vertex_buffer_view.StrideInBytes = sizeof(Vertex);
    vertex_buffer_view.SizeInBytes = render_scene.vertices_count * sizeof(Vertex);

    D3D12_INDEX_BUFFER_VIEW indices_index_buffer;
    indices_index_buffer.BufferLocation = render_scene.indices_buffer.GetGPUVirtualAddress();
    indices_index_buffer.Format = DXGI_FORMAT_R32_UINT;
    indices_index_buffer.SizeInBytes = render_scene.indices_count * sizeof(uint32_t);

    command_list.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // TODO: Bake it somewhere?
    command_list.list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
    command_list.list->IASetIndexBuffer(&indices_index_buffer);

    const std::optional<Pso> pso = renderer->GetPso(indirect_pso_id);

    if (pso.has_value())
    {
        command_list.list->SetPipelineState(pso.value().pso.get());
        command_list.list->SetGraphicsRootSignature(m_indirect_root_signature.get());

        // Argument buffer overflow. [ EXECUTION ERROR #744: EXECUTE_INDIRECT_INVALID_PARAMETERS]
        command_list.list->ExecuteIndirect(
            m_command_signature.get(), render_scene.primitives_count, m_command_buffer.resource.get(), 0, nullptr, 0);
    }
    else
    {
        LogError << "Can't find indirect buffer PSO\n";
    }

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            render_parameters.shadow_map_buffer.buffer.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        command_list.list->ResourceBarrier(1, &barrier);
    }

    render_parameters.command_queue->ExecuteCommandList(command_list);

    return true;
}
} // namespace ysn
