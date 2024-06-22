﻿module;

#include <DirectXMath.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <pix3.h>

#include <System/Assert.hpp>
#include <Graphics/ShaderSharedStructs.h>

// TODO(modules): remove this includes
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

export module yasno;

import std;
import yasno.camera_controller;
import graphics.techniques.convert_to_cubemap;
import graphics.engine_stats;
import graphics.techniques.shadow_map_pass;
import graphics.techniques.tonemap_pass;
import graphics.techniques.raytracing_pass;
import graphics.techniques.skybox_pass;
import graphics.techniques.forward_pass;
import graphics.render_scene;
import renderer.dxrenderer;
import renderer.gpu_buffer;
import renderer.command_queue;
import renderer.gpu_texture;
import renderer.raytracing_context;
import system.math;
import system.filesystem;
import system.window;
import system.application;
import system.gltf_loader;
import system.gui;
import system.game;
import system.game_input;
import system.helpers;

export namespace ysn
{
class Yasno : public Game
{
public:
    Yasno(const std::wstring& name, int width, int height, bool vsync = false);

    bool LoadContent() override;
    void UnloadContent() override;
    void Destroy() override;

protected:
    void OnUpdate(UpdateEventArgs& e) override;

    void OnRender(RenderEventArgs& e) override;

    void RenderUi();

    void OnResize(ResizeEventArgs& e) override;

private:
    GameInput game_input;
    RenderScene m_render_scene;

    bool CreateGpuCameraBuffer();
    bool CreateGpuSceneParametersBuffer();

    void UpdateGpuCameraBuffer();
    void UpdateGpuSceneParametersBuffer();

    void UpdateBufferResource(
        wil::com_ptr<ID3D12GraphicsCommandList2> commandList,
        ID3D12Resource** pDestinationResource,
        ID3D12Resource** pIntermediateResource,
        size_t numElements,
        size_t elementSize,
        const void* bufferData,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

    void ResizeDepthBuffer(int width, int height);
    void ResizeHdrBuffer(int width, int height);
    void ResizeBackBuffer(int width, int height);

    uint64_t m_fence_values[Window::BufferCount] = {};

    wil::com_ptr<ID3D12Resource> m_depth_buffer;
    wil::com_ptr<ID3D12Resource> m_back_buffer;
    // Two HDR buffers for history
    wil::com_ptr<ID3D12Resource> m_scene_color_buffer;
    wil::com_ptr<ID3D12Resource> m_scene_color_buffer_1;

    DescriptorHandle m_hdr_uav_descriptor_handle;
    DescriptorHandle m_backbuffer_uav_descriptor_handle;
    DescriptorHandle m_depth_dsv_descriptor_handle;
    DescriptorHandle m_hdr_rtv_descriptor_handle;

    RaytracingContext m_raytracing_context;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissors_rect;

    GpuTexture m_environment_texture;
    wil::com_ptr<ID3D12Resource> m_cubemap_texture;

    wil::com_ptr<ID3D12Resource> m_camera_gpu_buffer;
    wil::com_ptr<ID3D12Resource> m_scene_parameters_gpu_buffer;

    bool m_is_raster = true;
    bool m_is_indirect = false;
    bool m_is_raster_pressed = false;

    bool m_is_content_loaded = false;
    bool m_is_first_frame = true;
    bool m_reset_rtx_accumulation = true;
    bool m_is_rtx_accumulation_enabled = false;

    // Techniques
    ForwardPass m_forward_pass;
    ShadowMapPass m_shadow_pass;
    TonemapPostprocess m_tonemap_pass;
    RaytracingPass m_ray_tracing_pass;
    ConvertToCubemap m_convert_to_cubemap_pass;
    SkyboxPass m_skybox_pass;
    // GenerateMipsPass m_generate_mips_pass; // TODO(modules) : implement mips calculation
};
} // namespace ysn

module :private;

namespace ysn
{
std::optional<wil::com_ptr<ID3D12Resource>> CreateCubemapTexture()
{
    wil::com_ptr<ID3D12Resource> cubemap_gpu_texture;

    auto device = Application::Get().GetDevice();

    const auto size = 512;
    const auto format = DXGI_FORMAT_R16G16B16A16_FLOAT;

    D3D12_RESOURCE_DESC texture_desc = {};
    texture_desc.Format = format;
    texture_desc.Width = size;
    texture_desc.Height = size;
    texture_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    texture_desc.DepthOrArraySize = 6;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    const auto heap_properties_default = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    // const auto heap_properties_upload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    if (device->CreateCommittedResource(
            &heap_properties_default, D3D12_HEAP_FLAG_NONE, &texture_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&cubemap_gpu_texture)) !=
        S_OK)
    {
        LogError << "Can't create cubemap resource\n";
        return std::nullopt;
    }

#ifndef YSN_RELEASE
    cubemap_gpu_texture->SetName(L"Cubemap Texture");
#endif

    return cubemap_gpu_texture;
}

Yasno::Yasno(const std::wstring& name, int width, int height, bool vsync) :
    Game(name, width, height, vsync),
    m_viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))),
    m_scissors_rect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
{
}

void Yasno::UnloadContent()
{
    m_is_content_loaded = false;
}

void Yasno::Destroy()
{
    Game::Destroy();
    // ShutdownImgui(); // TODO(modules):
}

void Yasno::UpdateBufferResource(
    wil::com_ptr<ID3D12GraphicsCommandList2> commandList,
    ID3D12Resource** destination_resource,
    ID3D12Resource** intermediate_resource,
    size_t num_elements,
    size_t element_size,
    const void* buffer_data,
    D3D12_RESOURCE_FLAGS flags)
{
    auto device = Application::Get().GetDevice();

    size_t bufferSize = num_elements * element_size;

    const auto gpuBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);
    const auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    // Create a committed resource for the GPU resource in a default heap.
    ThrowIfFailed(device->CreateCommittedResource(
        &heap_properties,
        D3D12_HEAP_FLAG_NONE,
        &gpuBufferDesc,
        D3D12_RESOURCE_STATE_COMMON, // TODO(task): should be here
        // D3D12_RESOURCE_STATE_COPY_DEST?
        nullptr,
        IID_PPV_ARGS(destination_resource)));

    // Create an committed resource for the upload.
    if (buffer_data)
    {
        const auto gpuUploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
        const auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

        ThrowIfFailed(device->CreateCommittedResource(
            &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &gpuUploadBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(intermediate_resource)));

        D3D12_SUBRESOURCE_DATA subresource_ata = {};
        subresource_ata.pData = buffer_data;
        subresource_ata.RowPitch = bufferSize;
        subresource_ata.SlicePitch = bufferSize;

        UpdateSubresources(commandList.get(), *destination_resource, *intermediate_resource, 0, 0, 1, &subresource_ata);
    }
}

bool Yasno::CreateGpuCameraBuffer()
{
    wil::com_ptr<ID3D12Device5> device = Application::Get().GetDevice();

    D3D12_HEAP_PROPERTIES heap_properties = {};
    heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resource_desc = {};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Width = GpuCamera::GetGpuSize();
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc = {1, 0};
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    auto hr = device->CreateCommittedResource(
        &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_camera_gpu_buffer));

    if (hr != S_OK)
    {
        LogError << "Can't create camera GPU buffer\n";
        return false;
    }

    return true;
}

bool Yasno::CreateGpuSceneParametersBuffer()
{
    wil::com_ptr<ID3D12Device5> device = Application::Get().GetDevice();

    D3D12_HEAP_PROPERTIES heap_properties = {};
    heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resource_desc = {};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Width = GpuSceneParameters::GetGpuSize();
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc = {1, 0};
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    auto hr = device->CreateCommittedResource(
        &heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_scene_parameters_gpu_buffer));

    if (hr != S_OK)
    {
        LogError << "Can't create scene parameters GPU buffer\n";
        return false;
    }

    return true;
}

void Yasno::UpdateGpuCameraBuffer()
{
    void* data;
    m_camera_gpu_buffer->Map(0, nullptr, &data);

    auto* camera_data = static_cast<GpuCamera*>(data);

    DirectX::XMMATRIX view_projection = DirectX::XMMatrixIdentity();
    view_projection = XMMatrixMultiply(DirectX::XMMatrixIdentity(), m_render_scene.camera->GetViewMatrix());
    view_projection = XMMatrixMultiply(view_projection, m_render_scene.camera->GetProjectionMatrix());

    DirectX::XMVECTOR det;

    XMStoreFloat4x4(&camera_data->view_projection, view_projection);
    XMStoreFloat4x4(&camera_data->view, m_render_scene.camera->GetViewMatrix());
    XMStoreFloat4x4(&camera_data->projection, m_render_scene.camera->GetProjectionMatrix());
    XMStoreFloat4x4(&camera_data->view_inverse, XMMatrixInverse(&det, m_render_scene.camera->GetViewMatrix()));
    XMStoreFloat4x4(&camera_data->projection_inverse, XMMatrixInverse(&det, m_render_scene.camera->GetProjectionMatrix()));

    camera_data->position = m_render_scene.camera->GetPosition();
    camera_data->frame_number = m_frame_number;
    camera_data->frames_accumulated = m_rtx_frames_accumulated;
    camera_data->reset_accumulation = m_reset_rtx_accumulation ? 1 : 0;
    camera_data->accumulation_enabled = m_is_rtx_accumulation_enabled ? 1 : 0;

    m_camera_gpu_buffer->Unmap(0, nullptr);
}

void Yasno::UpdateGpuSceneParametersBuffer()
{
    void* data;
    m_scene_parameters_gpu_buffer->Map(0, nullptr, &data);

    auto* scene_parameters_data = static_cast<GpuSceneParameters*>(data);

    XMStoreFloat4x4(&scene_parameters_data->shadow_matrix, m_shadow_pass.shadow_matrix);
    scene_parameters_data->directional_light_color = m_render_scene.directional_light.color;
    scene_parameters_data->directional_light_direction = m_render_scene.directional_light.direction;
    scene_parameters_data->directional_light_intensity = m_render_scene.directional_light.intensity;
    scene_parameters_data->ambient_light_intensity = m_render_scene.environment_light.intensity;
    scene_parameters_data->shadows_enabled = (uint32_t)m_render_scene.directional_light.cast_shadow;

    m_scene_parameters_gpu_buffer->Unmap(0, nullptr);
}

bool Yasno::LoadContent()
{
    wil::com_ptr<ID3D12Device5> device = Application::Get().GetDevice();
    auto renderer = Application::Get().GetRenderer();

    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0; // TODO: WHY NOT D3D_ROOT_SIGNATURE_VERSION_1_2?????

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    // auto command_queue = Application::Get().GetCopyQueue();
    auto command_queue = Application::Get().GetDirectQueue();

    bool capture_loading_pix = false;

    if (capture_loading_pix)
    {
        PIXCaptureParameters pix_capture_parameters;
        pix_capture_parameters.GpuCaptureParameters.FileName = L"Yasno.pix";
        PIXSetTargetWindow(m_pWindow->GetWindowHandle());
        YSN_ASSERT(PIXBeginCapture(PIX_CAPTURE_GPU, &pix_capture_parameters) != S_OK);
    }

    wil::com_ptr<ID3D12GraphicsCommandList4> command_list = command_queue->GetCommandList("Load Content");

    bool load_result = false;

    {
        LoadingParameters loading_parameters;
        loading_parameters.model_modifier = XMMatrixScaling(0.01f, 0.01f, 0.01f);
        load_result = LoadGltfFromFile(m_render_scene, GetVirtualFilesystemPath(L"assets/Sponza/Sponza.gltf"), loading_parameters);
    }

    //{
    //	LoadingParameters loading_parameters;
    //	load_result = LoadGltfFromFile(m_render_scene, GetVirtualFilesystemPath(L"assets/DamagedHelmet/DamagedHelmet.gltf"), loading_parameters);
    //}

    //{
    //	LoadingParameters loading_parameters;
    //	loading_parameters.model_modifier = XMMatrixScaling(0.01f, 0.01f, 0.01f);
    //	load_result = LoadGltfFromFile(m_render_scene, GetVirtualFilesystemPath(L"assets/Bistro/Bistro.gltf"), loading_parameters);
    //}

    //{
    //	LoadingParameters loading_parameters;
    //	load_result = LoadGltfFromFile(m_render_scene, GetVirtualFilesystemPath(L"assets/Sponza_New/NewSponza_Main_glTF_002.gltf"), loading_parameters);
    //}

    // Finish index buffer
    {
        // Index buffer
        {
            const uint32_t indices_buffer_size = m_render_scene.indices_count * sizeof(uint32_t);

            GpuBufferCreateInfo create_info{
                .size = indices_buffer_size, .heap_type = D3D12_HEAP_TYPE_DEFAULT, .state = D3D12_RESOURCE_STATE_COPY_DEST};

            const auto indices_buffer_result = CreateGpuBuffer(create_info, "Indices Buffer");

            if (!indices_buffer_result.has_value())
            {
                LogError << "Can't create indices buffer\n";
                return false;
            }

            m_render_scene.indices_buffer = indices_buffer_result.value();

            std::vector<uint32_t> all_indices_buffer;
            all_indices_buffer.reserve(m_render_scene.indices_count);

            for (auto& model : m_render_scene.models)
            {
                for (auto& mesh : model.meshes)
                {
                    for (auto& primitive : mesh.primitives)
                    {
                        primitive.index_buffer_view.BufferLocation =
                            m_render_scene.indices_buffer.GetGPUVirtualAddress() + all_indices_buffer.size() * sizeof(uint32_t);
                        primitive.index_buffer_view.SizeInBytes = static_cast<uint32_t>(primitive.indices.size()) * sizeof(uint32_t);
                        primitive.index_buffer_view.Format = DXGI_FORMAT_R32_UINT;

                        primitive.index_count = static_cast<uint32_t>(primitive.indices.size());

                        // Append indices
                        all_indices_buffer.insert(all_indices_buffer.end(), primitive.indices.begin(), primitive.indices.end());
                    }
                }
            }

            UploadToGpuBuffer(
                command_list, m_render_scene.indices_buffer, all_indices_buffer.data(), {}, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }

        // Vertex Buffer
        {
            const uint32_t vertices_buffer_size = m_render_scene.vertices_count * sizeof(Vertex);

            GpuBufferCreateInfo create_info{
                .size = vertices_buffer_size, .heap_type = D3D12_HEAP_TYPE_DEFAULT, .state = D3D12_RESOURCE_STATE_COPY_DEST};

            const auto vertices_buffer_result = CreateGpuBuffer(create_info, "Vertices Buffer");

            if (!vertices_buffer_result.has_value())
            {
                LogError << "Can't create vertices buffer\n";
                return false;
            }

            m_render_scene.vertices_buffer = vertices_buffer_result.value();

            std::vector<Vertex> all_vertices_buffer;
            all_vertices_buffer.reserve(m_render_scene.vertices_count);

            for (auto& model : m_render_scene.models)
            {
                for (auto& mesh : model.meshes)
                {
                    for (auto& primitive : mesh.primitives)
                    {
                        primitive.vertex_buffer_view.BufferLocation =
                            m_render_scene.vertices_buffer.GetGPUVirtualAddress() + all_vertices_buffer.size() * sizeof(Vertex);
                        primitive.vertex_buffer_view.SizeInBytes = UINT(primitive.vertices.size() * sizeof(Vertex));
                        primitive.vertex_buffer_view.StrideInBytes = sizeof(Vertex);

                        primitive.vertex_count = static_cast<uint32_t>(primitive.vertices.size());

                        // Append vertices
                        all_vertices_buffer.insert(all_vertices_buffer.end(), primitive.vertices.begin(), primitive.vertices.end());
                    }
                }
            }

            UploadToGpuBuffer(
                command_list, m_render_scene.vertices_buffer, all_vertices_buffer.data(), {}, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }

        // Material buffer
        {
            GpuBufferCreateInfo create_info{
                .size = m_render_scene.materials_count * sizeof(SurfaceShaderParameters),
                .heap_type = D3D12_HEAP_TYPE_DEFAULT,
                .state = D3D12_RESOURCE_STATE_COPY_DEST};

            const auto materials_buffer_result = CreateGpuBuffer(create_info, "Materials Buffer");

            if (!materials_buffer_result.has_value())
            {
                LogError << "Can't create materials buffer\n";
                return false;
            }

            m_render_scene.materials_buffer = materials_buffer_result.value();

            std::vector<SurfaceShaderParameters> all_materials_buffer;
            all_materials_buffer.reserve(m_render_scene.materials_count);

            for (auto& model : m_render_scene.models)
            {
                for (auto& material : model.shader_parameters)
                {
                    all_materials_buffer.push_back(material);
                }
            }

            UploadToGpuBuffer(
                command_list, m_render_scene.materials_buffer, all_materials_buffer.data(), {}, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            // Create SRV
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srv_desc.Buffer.FirstElement = 0;
            srv_desc.Buffer.NumElements = static_cast<UINT>(all_materials_buffer.size());
            srv_desc.Buffer.StructureByteStride = sizeof(SurfaceShaderParameters);
            srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            m_render_scene.materials_buffer_srv = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
            renderer->GetDevice()->CreateShaderResourceView(
                m_render_scene.materials_buffer.resource.get(), &srv_desc, m_render_scene.materials_buffer_srv.cpu);
        }

        // Instance buffer
        {
            GpuBufferCreateInfo create_info{
                .size = m_render_scene.primitives_count * sizeof(RenderInstanceData),
                .heap_type = D3D12_HEAP_TYPE_DEFAULT,
                .state = D3D12_RESOURCE_STATE_COPY_DEST};

            const auto per_instance_buffer_result = CreateGpuBuffer(create_info, "RenderInstance Buffer");

            if (!per_instance_buffer_result.has_value())
            {
                LogError << "Can't create RenderInstance buffer\n";
                return false;
            }

            m_render_scene.instance_buffer = per_instance_buffer_result.value();

            std::vector<RenderInstanceData> per_instance_data_buffer;
            per_instance_data_buffer.reserve(m_render_scene.primitives_count);

            uint32_t total_indices = 0;
            uint32_t total_vertices = 0;

            for (Model& model : m_render_scene.models)
            {
                for (int mesh_id = 0; mesh_id < model.meshes.size(); mesh_id++)
                {
                    Mesh& mesh = model.meshes[mesh_id];

                    for (Primitive& primitive : mesh.primitives)
                    {
                        RenderInstanceData instance_data;
                        if (primitive.material_id == -1)
                        {
                            LogInfo << "Material ID is negative when filling instance data buffer - investigate";
                            // TODO: Assign "broken" material
                            instance_data.material_id = 0;
                        }
                        else
                        {
                            instance_data.material_id = primitive.material_id;
                        }

                        instance_data.model_matrix = model.transforms[mesh_id].transform;
                        instance_data.vertices_before = total_vertices;
                        instance_data.indices_before = total_indices;

                        // Need for indirect commands filling later
                        primitive.global_vertex_offset = instance_data.vertices_before;
                        primitive.global_index_offset = instance_data.indices_before;

                        total_indices += primitive.index_count;
                        total_vertices += primitive.vertex_count;

                        per_instance_data_buffer.push_back(instance_data);
                    }
                }
            }

            UploadToGpuBuffer(
                command_list, m_render_scene.instance_buffer, per_instance_data_buffer.data(), {}, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

            // Create SRV
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srv_desc.Buffer.FirstElement = 0;
            srv_desc.Buffer.NumElements = static_cast<UINT>(m_render_scene.primitives_count);
            srv_desc.Buffer.StructureByteStride = sizeof(RenderInstanceData);
            srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            m_render_scene.instance_buffer_srv = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
            renderer->GetDevice()->CreateShaderResourceView(
                m_render_scene.instance_buffer.resource.get(), &srv_desc, m_render_scene.instance_buffer_srv.cpu);
        }
    }

    for (auto& model : m_render_scene.models)
    {
        for (auto& mesh : model.meshes)
        {
            for (auto& primitive : mesh.primitives)
            {
                m_forward_pass.CompilePrimitivePso(primitive, model.materials);
                m_shadow_pass.CompilePrimitivePso(primitive, model.materials);
            }
        }
    }

    m_raytracing_context.CreateAccelerationStructures(command_list, m_render_scene);

    if (!m_tonemap_pass.Initialize())
    {
        LogError << "Can't initialize tonemap pass\n";
        return false;
    }

    // if (!m_convert_to_cubemap_pass.Initialize())
    //{
    //	LogError << "Can't initialize convert to cubemap pass\n";
    //	return false;
    // }

    if (!m_skybox_pass.Initialize())
    {
        LogError << "Can't initialize skybox pass\n";
        return false;
    }

    // TODO(modules); restore
    // InitializeImgui(m_pWindow, renderer);

    const auto enviroment_hdr_descriptor_handle = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

    {
        LoadTextureParameters parameter;
        parameter.filename = "assets/HDRI/drackenstein_quarry_puresky_4k.hdr";
        parameter.command_list = command_list;
        parameter.generate_mips = false;
        parameter.is_srgb = false;
        const auto env_texture = LoadTextureFromFile(parameter);

        if (!env_texture.has_value())
        {
            return false;
        }

        m_environment_texture = *env_texture;
    }

    m_hdr_uav_descriptor_handle = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
    m_backbuffer_uav_descriptor_handle = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
    m_depth_dsv_descriptor_handle = renderer->GetDsvDescriptorHeap()->GetNewHandle();

    if (!CreateGpuCameraBuffer())
    {
        LogError << "Yasno app can't create GPU camera buffer\n";
        return false;
    }

    if (!CreateGpuSceneParametersBuffer())
    {
        LogError << "Yasno app can't create GPU scene parameters buffer\n";
        return false;
    }

    if (!m_forward_pass.Initialize(
            m_render_scene, m_camera_gpu_buffer, m_scene_parameters_gpu_buffer, m_render_scene.instance_buffer, command_list))
    {
        LogError << "Can't initialize forward pass\n";
        return false;
    }

    auto fence_value = command_queue->ExecuteCommandList(command_list);
    command_queue->WaitForFenceValue(fence_value);

    if (capture_loading_pix)
    {
        YSN_ASSERT(PIXEndCapture(false) != S_OK);
    }

    m_is_content_loaded = true;

    // Resize/Create the depth buffer.
    ResizeDepthBuffer(GetClientWidth(), GetClientHeight());
    ResizeHdrBuffer(GetClientWidth(), GetClientHeight());
    ResizeBackBuffer(GetClientWidth(), GetClientHeight());

    // Setup techniques
    m_shadow_pass.Initialize(Application::Get().GetRenderer());

    if (!m_ray_tracing_pass.Initialize(
            Application::Get().GetRenderer(),
            m_scene_color_buffer,
            m_scene_color_buffer_1,
            m_raytracing_context,
            m_camera_gpu_buffer,
            m_render_scene.vertices_buffer.resource,
            m_render_scene.indices_buffer.resource,
            m_render_scene.materials_buffer.resource,
            m_render_scene.instance_buffer.resource,
            m_render_scene.vertices_count,
            m_render_scene.indices_count,
            m_render_scene.materials_count,
            m_render_scene.primitives_count))
    {
        LogError << "Can't initialize raytracing pass\n";
        return false;
    }

    // Setup camera
    m_render_scene.camera = std::make_shared<ysn::Camera>();
    m_render_scene.camera->SetPosition({-5, 0, 0});
    m_render_scene.camera_controler.pCamera = m_render_scene.camera;

    game_input.Initialize(m_pWindow->GetWindowHandle());

    // Post init
    const auto cubemap_texture_result = CreateCubemapTexture();
    if (!cubemap_texture_result.has_value())
    {
        LogError << "Can't create cubemap\n";
        return false;
    }

    m_cubemap_texture = cubemap_texture_result.value();

    return true;
}

void Yasno::ResizeDepthBuffer(int width, int height)
{
    if (m_is_content_loaded)
    {
        // Flush any GPU commands that might be referencing the depth buffer.
        Application::Get().Flush();

        width = std::max(1, width);
        height = std::max(1, height);

        wil::com_ptr<ID3D12Device5> device = Application::Get().GetDevice();

        // Resize screen dependent resources.
        // Create a depth buffer.
        D3D12_CLEAR_VALUE optimized_clear_valuie = {};
        optimized_clear_valuie.Format = DXGI_FORMAT_D32_FLOAT;
        optimized_clear_valuie.DepthStencil = {1.0f, 0};

        const CD3DX12_HEAP_PROPERTIES heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        const CD3DX12_RESOURCE_DESC resource_desc =
            CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        ThrowIfFailed(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &optimized_clear_valuie,
            IID_PPV_ARGS(&m_depth_buffer)));

        // Update the depth-stencil view.
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
        dsv.Format = DXGI_FORMAT_D32_FLOAT;
        dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv.Texture2D.MipSlice = 0;
        dsv.Flags = D3D12_DSV_FLAG_NONE;

        device->CreateDepthStencilView(m_depth_buffer.get(), &dsv, m_depth_dsv_descriptor_handle.cpu);
    }
}

void Yasno::ResizeHdrBuffer(int width, int height)
{
    if (m_is_content_loaded)
    {
        // Flush any GPU commands that might be referencing the depth buffer.
        Application::Get().Flush();

        width = std::max(1, width);
        height = std::max(1, height);

        auto device = Application::Get().GetDevice();
        auto renderer = Application::Get().GetRenderer();

        D3D12_CLEAR_VALUE optimized_clear_valuie = {};
        optimized_clear_valuie.Format = Application::Get().GetRenderer()->GetHdrFormat();
        optimized_clear_valuie.Color[0] = 0.0f;
        optimized_clear_valuie.Color[1] = 0.0f;
        optimized_clear_valuie.Color[2] = 0.0f;
        optimized_clear_valuie.Color[3] = 0.0f;

        const auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        const auto resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(
            Application::Get().GetRenderer()->GetHdrFormat(), width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        ThrowIfFailed(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            &optimized_clear_valuie,
            IID_PPV_ARGS(&m_scene_color_buffer)));

        ThrowIfFailed(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            &optimized_clear_valuie,
            IID_PPV_ARGS(&m_scene_color_buffer_1)));

#ifndef YSN_RELEASE
        m_scene_color_buffer->SetName(L"Scene Color 0");
        m_scene_color_buffer_1->SetName(L"Scene Color 1");
#endif

        D3D12_RENDER_TARGET_VIEW_DESC rtv = {};
        rtv.Format = Application::Get().GetRenderer()->GetHdrFormat();
        rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtv.Texture2D.MipSlice = 0;

        m_hdr_rtv_descriptor_handle = renderer->GetRtvDescriptorHeap()->GetNewHandle();

        device->CreateRenderTargetView(m_scene_color_buffer.get(), &rtv, m_hdr_rtv_descriptor_handle.cpu);

        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = Application::Get().GetRenderer()->GetHdrFormat();
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

            device->CreateUnorderedAccessView(m_scene_color_buffer.get(), nullptr, &uavDesc, m_hdr_uav_descriptor_handle.cpu);
        }

        //{
        //	auto scene_color_buffer_1_uav_handle = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

        //	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        //	uavDesc.Format = Application::Get().GetRenderer()->GetHdrFormat();
        //	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        //	device->CreateUnorderedAccessView(m_scene_color_buffer_1.get(), nullptr, &uavDesc, scene_color_buffer_1_uav_handle.cpu);
        //}
    }
}

void Yasno::ResizeBackBuffer(int width, int height)
{
    if (m_is_content_loaded)
    {
        // Flush any GPU commands that might be referencing the depth buffer.
        Application::Get().Flush();

        width = std::max(1, width);
        height = std::max(1, height);

        auto device = Application::Get().GetDevice();

        D3D12_CLEAR_VALUE optimized_clear_valuie = {};
        optimized_clear_valuie.Format = Application::Get().GetRenderer()->GetBackBufferFormat();
        optimized_clear_valuie.Color[0] = 0.0f;
        optimized_clear_valuie.Color[1] = 0.0f;
        optimized_clear_valuie.Color[2] = 0.0f;
        optimized_clear_valuie.Color[3] = 0.0f;

        const auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        const auto resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(
            Application::Get().GetRenderer()->GetBackBufferFormat(),
            width,
            height,
            1,
            0,
            1,
            0,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        ThrowIfFailed(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            &optimized_clear_valuie,
            IID_PPV_ARGS(&m_back_buffer)));

#ifndef YSN_RELEASED
        m_back_buffer->SetName(L"Back Buffer");
#endif

        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = Application::Get().GetRenderer()->GetBackBufferFormat();
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

            device->CreateUnorderedAccessView(m_back_buffer.get(), nullptr, &uavDesc, m_backbuffer_uav_descriptor_handle.cpu);
        }
    }
}

void Yasno::OnResize(ResizeEventArgs& e)
{
    if (e.Width != GetClientWidth() || e.Height != GetClientHeight())
    {
        Game::OnResize(e);

        m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(e.Width), static_cast<float>(e.Height));

        ResizeDepthBuffer(e.Width, e.Height);
        ResizeHdrBuffer(e.Width, e.Height);
        ResizeBackBuffer(e.Width, e.Height);
    }
}

void Yasno::OnUpdate(UpdateEventArgs& e)
{
    static double totalTime = 0.0;

    Game::OnUpdate(e);

    totalTime += e.ElapsedTime;
    m_frame_number += 1;

    // if (totalTime > 1.0)
    //{
    //	engine_stats::fps = uint32_t(m_frame_number / totalTime);

    //	frameCount = 0;
    //	totalTime = 0.0;
    //}

    auto kb = game_input.keyboard->GetState();

    auto mouse = game_input.mouse->GetState();
    game_input.mouse->SetMode(mouse.rightButton ? DirectX::Mouse::MODE_RELATIVE : DirectX::Mouse::MODE_ABSOLUTE);

    if (mouse.positionMode == DirectX::Mouse::MODE_RELATIVE)
    {
        m_render_scene.camera_controler.MoveMouse(mouse.x, mouse.y);

        m_render_scene.camera_controler.m_IsMovementBoostActive = kb.IsKeyDown(DirectX::Keyboard::LeftShift);

        if (kb.IsKeyDown(DirectX::Keyboard::W))
        {
            m_render_scene.camera_controler.MoveForward(static_cast<float>(e.ElapsedTime));
        }
        if (kb.IsKeyDown(DirectX::Keyboard::A))
        {
            m_render_scene.camera_controler.MoveLeft(static_cast<float>(e.ElapsedTime));
        }
        if (kb.IsKeyDown(DirectX::Keyboard::S))
        {
            m_render_scene.camera_controler.MoveBackwards(static_cast<float>(e.ElapsedTime));
        }
        if (kb.IsKeyDown(DirectX::Keyboard::D))
        {
            m_render_scene.camera_controler.MoveRight(static_cast<float>(e.ElapsedTime));
        }
        if (kb.IsKeyDown(DirectX::Keyboard::Space))
        {
            m_render_scene.camera_controler.MoveUp(static_cast<float>(e.ElapsedTime));
        }
        if (kb.IsKeyDown(DirectX::Keyboard::LeftControl))
        {
            m_render_scene.camera_controler.MoveDown(static_cast<float>(e.ElapsedTime));
        }
    }

    if (kb.IsKeyDown(DirectX::Keyboard::Escape))
    {
        Application::Get().Quit(0);
    }

    {
        if (kb.IsKeyDown(DirectX::Keyboard::R) && !m_is_raster_pressed)
        {
            m_is_raster = !m_is_raster;
            m_is_raster_pressed = true;
        }

        if (kb.IsKeyUp(DirectX::Keyboard::R))
        {
            m_is_raster_pressed = false;
        }
    }

    if (m_render_scene.camera_controler.IsMoved())
    {
        m_reset_rtx_accumulation = true;
    }

    m_render_scene.camera->SetAspectRatio(GetClientWidth() / static_cast<float>(GetClientHeight()));
    m_render_scene.camera->Update();

    // todo(modules):
#ifndef YSN_RELEASE
    // Track shader updates
    // Application::Get().GetRenderer()->GetShaderStorage()->VerifyAnyShaderChanged();
#endif

    // Clear stage GPU resources
    Application::Get().GetRenderer()->stage_heap.clear();
}

void Yasno::RenderUi()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit", "Esc"))
            {
                Application::Get().Quit(0);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    {
        ImGui::Begin(CONTROLS_NAME.c_str());

        ImGui::Checkbox("Indirect", &m_is_indirect);

        if (ImGui::CollapsingHeader("Lighting"), ImGuiTreeNodeFlags_DefaultOpen)
        {
            static float color[3] = {
                m_render_scene.directional_light.color.x,
                m_render_scene.directional_light.color.y,
                m_render_scene.directional_light.color.z};
            ImGui::ColorEdit3("Color", color, ImGuiColorEditFlags_Float);
            m_render_scene.directional_light.color.x = color[0];
            m_render_scene.directional_light.color.y = color[1];
            m_render_scene.directional_light.color.z = color[2];

            static float dir[3] = {
                m_render_scene.directional_light.direction.x,
                m_render_scene.directional_light.direction.y,
                m_render_scene.directional_light.direction.z};
            ImGui::InputFloat3("Direction", dir);
            m_render_scene.directional_light.direction.x = dir[0];
            m_render_scene.directional_light.direction.y = dir[1];
            m_render_scene.directional_light.direction.z = dir[2];

            m_shadow_pass.UpdateLight(m_render_scene.directional_light);

            ImGui::InputFloat("Intensity", &m_render_scene.directional_light.intensity, 0.0f, 1000.0f);
            ImGui::InputFloat("Env Light Intensity", &m_render_scene.environment_light.intensity, 0.0f, 1000.0f);
        }

        if (ImGui::CollapsingHeader("Shadows"), ImGuiTreeNodeFlags_DefaultOpen)
        {
            ImGui::Checkbox("Enabled", &m_render_scene.directional_light.cast_shadow);
        }

        if (ImGui::CollapsingHeader("Tonemapping"), ImGuiTreeNodeFlags_DefaultOpen)
        {
            const char* items[] = {
                "None",
                "Reinhard",
                "ACES",
            };
            static int item_current = (int)m_tonemap_pass.tonemap_method;
            ImGui::Combo("Tonemapper", &item_current, items, IM_ARRAYSIZE(items));
            m_tonemap_pass.tonemap_method = static_cast<TonemapMethod>(item_current);

            ImGui::InputFloat("Exposure", &m_tonemap_pass.exposure, 0.0, 1000.0);
        }

        if (ImGui::CollapsingHeader("Camera"), ImGuiTreeNodeFlags_DefaultOpen)
        {
            ImGui::InputFloat("Speed", &m_render_scene.camera_controler.mouse_speed, 0.0f, 1000.0f);
            ImGui::InputFloat("FOV", &m_render_scene.camera->fov, 0.0f, 1000.0f);
        }

        if (ImGui::CollapsingHeader("System"), ImGuiTreeNodeFlags_DefaultOpen)
        {
            ImGui::Checkbox("Vsync", &m_vsync);
        }

        ImGui::End();
    }

    {
        ImGui::Begin(STATS_NAME.c_str());

        // ImGui::Text(std::format("FPS: {}", engine_stats::fps).c_str()); // todo(modules);

        if (ImGui::CollapsingHeader("Mode"), ImGuiTreeNodeFlags_DefaultOpen)
        {
            if (m_is_raster)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
                ImGui::Text("Rasterization");
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 215, 0, 255));
                ImGui::Text("Pathtracing");
                ImGui::Checkbox("Temporal Accumulation", &m_is_rtx_accumulation_enabled);
                ImGui::PopStyleColor();
            }
        }

        ImGui::End();
    }
}

void Yasno::OnRender(RenderEventArgs& e)
{
    Game::OnRender(e);

    if (GetClientWidth() < 8 || GetClientHeight() < 8)
    {
        // Skip render to very small output
        return;
    }

    // Render GUI
    {
        // ImguiPrepareNewFrame(); // todo(modules):
        ImGuizmo::BeginFrame();

        RenderUi();

        ImGuizmo::SetRect(0, 0, static_cast<float>(GetClientWidth()), static_cast<float>(GetClientHeight()));

        XMFLOAT4X4 view;
        XMFLOAT4X4 projection;
        XMFLOAT4X4 identity;

        XMStoreFloat4x4(&view, m_render_scene.camera->GetViewMatrix());
        XMStoreFloat4x4(&projection, m_render_scene.camera->GetProjectionMatrix());
        XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());

        ImGuizmo::Manipulate(&view.m[0][0], &projection.m[0][0], ImGuizmo::TRANSLATE, ImGuizmo::WORLD, &identity.m[0][0]);
    }

    std::shared_ptr<ysn::CommandQueue> command_queue = Application::Get().GetDirectQueue();
    auto renderer = Application::Get().GetRenderer();

    UINT current_backbuffer_index = m_pWindow->GetCurrentBackBufferIndex();
    wil::com_ptr<ID3D12Resource> current_back_buffer = m_pWindow->GetCurrentBackBuffer();
    D3D12_CPU_DESCRIPTOR_HANDLE backbuffer_handle = m_pWindow->GetCurrentRenderTargetView();

    // m_convert_to_cubemap_pass.EquirectangularToCubemap(command_queue, m_environment_texture, m_cubemap_texture);

    if (m_is_raster)
    {
        ShadowRenderParameters parameters;
        parameters.command_queue = command_queue;
        parameters.scene_parameters_gpu_buffer = m_scene_parameters_gpu_buffer;

        m_shadow_pass.Render(m_render_scene, parameters);
    }

    UpdateGpuCameraBuffer();
    UpdateGpuSceneParametersBuffer();

    if (m_is_raster)
    {
        {
            wil::com_ptr<ID3D12GraphicsCommandList4> command_list = command_queue->GetCommandList("Clear before forward");

            // Clear the render targets.
            FLOAT clear_color[] = {44.0f / 255.0f, 58.f / 255.0f, 74.0f / 255.0f, 1.0f};

            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                current_back_buffer.get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            command_list->ResourceBarrier(1, &barrier);

            command_list->ClearRenderTargetView(backbuffer_handle, clear_color, 0, nullptr);
            command_list->ClearDepthStencilView(m_depth_dsv_descriptor_handle.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            command_list->ClearRenderTargetView(m_hdr_rtv_descriptor_handle.cpu, clear_color, 0, nullptr);

            command_queue->ExecuteCommandList(command_list);
        }

        {
            ForwardPassRenderParameters render_parameters;
            render_parameters.command_queue = command_queue;
            render_parameters.cbv_srv_uav_heap = renderer->GetCbvSrvUavDescriptorHeap();
            render_parameters.scene_color_buffer = m_scene_color_buffer;
            render_parameters.hdr_rtv_descriptor_handle = m_hdr_rtv_descriptor_handle;
            render_parameters.dsv_descriptor_handle = m_depth_dsv_descriptor_handle;
            render_parameters.viewport = m_viewport;
            render_parameters.scissors_rect = m_scissors_rect;
            render_parameters.camera_gpu_buffer = m_camera_gpu_buffer;
            render_parameters.backbuffer_handle = backbuffer_handle;
            render_parameters.current_back_buffer = current_back_buffer;
            render_parameters.shadow_map_buffer = m_shadow_pass.shadow_map_buffer;
            render_parameters.scene_parameters_gpu_buffer = m_scene_parameters_gpu_buffer;

            if (m_is_indirect)
            {
                m_forward_pass.RenderIndirect(m_render_scene, render_parameters);
            }
            else
            {
                m_forward_pass.Render(m_render_scene, render_parameters);
            }
        }
    }
    else
    {
        if (m_is_rtx_accumulation_enabled || !m_reset_rtx_accumulation)
        {
            m_rtx_frames_accumulated += 1;
        }
        else
        {
            m_rtx_frames_accumulated = 0;
            m_reset_rtx_accumulation = true;
        }

        wil::com_ptr<ID3D12GraphicsCommandList4> command_list = command_queue->GetCommandList("RTX Pass");

        // Clear the render targets.
        {
            FLOAT clear_color[] = {255.0f / 255.0f, 58.f / 255.0f, 74.0f / 255.0f, 1.0f};

            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                current_back_buffer.get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            command_list->ResourceBarrier(1, &barrier);

            command_list->ClearRenderTargetView(backbuffer_handle, clear_color, 0, nullptr);
            command_list->ClearDepthStencilView(m_depth_dsv_descriptor_handle.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            command_list->ClearRenderTargetView(m_hdr_rtv_descriptor_handle.cpu, clear_color, 0, nullptr);
        }

        m_ray_tracing_pass.Execute(
            Application::Get().GetRenderer(), command_list, GetClientWidth(), GetClientHeight(), m_scene_color_buffer, m_camera_gpu_buffer);

        command_queue->ExecuteCommandList(command_list);
    }

    if (m_is_raster)
    {
        SkyboxPassParameters skybox_parameters;
        skybox_parameters.command_queue = command_queue;
        skybox_parameters.cbv_srv_uav_heap = renderer->GetCbvSrvUavDescriptorHeap();
        skybox_parameters.scene_color_buffer = m_scene_color_buffer;
        skybox_parameters.hdr_rtv_descriptor_handle = m_hdr_rtv_descriptor_handle;
        skybox_parameters.dsv_descriptor_handle = m_depth_dsv_descriptor_handle;
        skybox_parameters.viewport = m_viewport;
        skybox_parameters.scissors_rect = m_scissors_rect;
        skybox_parameters.equirectangular_texture = &m_environment_texture;
        skybox_parameters.camera_gpu_buffer = m_camera_gpu_buffer;

        m_skybox_pass.RenderSkybox(&skybox_parameters);
    }

    {
        TonemapPostprocessParameters tonemap_parameters;
        tonemap_parameters.command_queue = command_queue;
        tonemap_parameters.cbv_srv_uav_heap = renderer->GetCbvSrvUavDescriptorHeap();
        tonemap_parameters.scene_color_buffer = m_scene_color_buffer;
        tonemap_parameters.hdr_uav_descriptor_handle = m_hdr_uav_descriptor_handle;
        tonemap_parameters.backbuffer_uav_descriptor_handle = m_backbuffer_uav_descriptor_handle;
        tonemap_parameters.backbuffer_width = GetClientWidth();
        tonemap_parameters.backbuffer_height = GetClientHeight();

        m_tonemap_pass.Render(&tonemap_parameters);
    }

    {
        auto commandListCopyBackBuffer = command_queue->GetCommandList("Copy Backbuffer");

        D3D12_BOX sourceRegion;
        sourceRegion.left = 0;
        sourceRegion.top = 0;
        sourceRegion.right = GetClientWidth();
        sourceRegion.bottom = GetClientHeight();
        sourceRegion.front = 0;
        sourceRegion.back = 1;

        CD3DX12_TEXTURE_COPY_LOCATION Dst(current_back_buffer.get());
        CD3DX12_TEXTURE_COPY_LOCATION Src(m_back_buffer.get());

        CD3DX12_RESOURCE_BARRIER barrier0 = CD3DX12_RESOURCE_BARRIER::Transition(
            m_back_buffer.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

        CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
            current_back_buffer.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);

        commandListCopyBackBuffer->ResourceBarrier(1, &barrier0);
        commandListCopyBackBuffer->ResourceBarrier(1, &barrier1);

        commandListCopyBackBuffer->CopyTextureRegion(&Dst, 0, 0, 0, &Src, &sourceRegion);

        CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
            current_back_buffer.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
        CD3DX12_RESOURCE_BARRIER barrier3 = CD3DX12_RESOURCE_BARRIER::Transition(
            m_back_buffer.get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        commandListCopyBackBuffer->ResourceBarrier(1, &barrier2);
        commandListCopyBackBuffer->ResourceBarrier(1, &barrier3);

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_scene_color_buffer.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandListCopyBackBuffer->ResourceBarrier(1, &barrier);

        command_queue->ExecuteCommandList(commandListCopyBackBuffer);
    }

    {
        auto command_list = command_queue->GetCommandList("Imgui");

        ID3D12DescriptorHeap* ppHeaps[] = {Application::Get().GetRenderer()->GetCbvSrvUavDescriptorHeap()->GetHeapPtr()};
        command_list->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
        command_list->OMSetRenderTargets(1, &backbuffer_handle, FALSE, &m_depth_dsv_descriptor_handle.cpu);

        // ImguiRenderFrame(command_list); todo(modules):

        command_queue->ExecuteCommandList(command_list);
    }

    // Present
    {
        auto command_list = command_queue->GetCommandList("Present");

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            current_back_buffer.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        command_list->ResourceBarrier(1, &barrier);

        m_fence_values[current_backbuffer_index] = command_queue->ExecuteCommandList(command_list);

        current_backbuffer_index = m_pWindow->Present();

        command_queue->WaitForFenceValue(m_fence_values[current_backbuffer_index]);
    }

    m_is_first_frame = false;
    m_reset_rtx_accumulation = false;
}
} // namespace ysn