module;

#include <wil/com.h>
#include <d3d12.h>
#include <DirectXMath.h>

export module renderer.raytracing_context;

import std;
import system.application;
import system.gltf_loader;
import graphics.render_scene;
import renderer.dxrenderer;
import renderer.descriptor_heap;
import renderer.nv.rt_pipeline_generator;
import renderer.nv.rs_generator;
import renderer.nv.sbt_generator;
import renderer.nv.tlas_generator;
import renderer.nv.blas_generator;
import renderer.dxr_helper;

export namespace ysn
{
struct AccelerationStructureBuffers
{
    wil::com_ptr<ID3D12Resource> scratch;       // Scratch memory for AS builder
    wil::com_ptr<ID3D12Resource> result;        // Where the AS is
    wil::com_ptr<ID3D12Resource> instance_desc; // Hold the matrices of the instances

    DescriptorHandle tlas_srv;
};

struct BLASVertexInput
{
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
    D3D12_INDEX_BUFFER_VIEW index_buffer_view;

    uint32_t vertex_count;
    uint32_t index_count;
};

struct TlasInput
{
    wil::com_ptr<ID3D12Resource> blas;
    DirectX::XMMATRIX transform;
    uint32_t instance_id = 0;
};

struct RaytracingContext
{
    AccelerationStructureBuffers CreateBottomLevelAS(
        wil::com_ptr<ID3D12Device5> device, wil::com_ptr<ID3D12GraphicsCommandList4> command_list, std::vector<BLASVertexInput> vertex_buffers);

    void CreateTopLevelAS(
        wil::com_ptr<ID3D12Device5> device, wil::com_ptr<ID3D12GraphicsCommandList4> command_list, const std::vector<TlasInput>& instances);

    void CreateTlasSrv(std::shared_ptr<ysn::DxRenderer> renderer);

    void CreateAccelerationStructures(wil::com_ptr<ID3D12GraphicsCommandList4> command_list, const RenderScene& render_scene);

    nv_helpers_dx12::TopLevelASGenerator tlas_generator;

    std::vector<wil::com_ptr<ID3D12Resource>> blas_res;

    AccelerationStructureBuffers tlas_buffers;
    std::vector<TlasInput> instances;
};
} // namespace ysn

module :private;

// TODO(postrtx): remove this
ID3D12Resource* CreateBuffer(
    ID3D12Device* m_device, uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps)
{
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Alignment = 0;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Flags = flags;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.Height = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.SampleDesc.Quality = 0;
    bufDesc.Width = size;

    ID3D12Resource* pBuffer;
    // todo(module): check for result
    m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&pBuffer));
    return pBuffer;
}

// Create a bottom-level acceleration structure based on a list of vertex
// buffers in GPU memory along with their vertex count. The build is then done
// in 3 steps: gathering the geometry, computing the sizes of the required
// buffers, and building the actual AS
ysn::AccelerationStructureBuffers ysn::RaytracingContext::CreateBottomLevelAS(
    wil::com_ptr<ID3D12Device5> device,
    wil::com_ptr<ID3D12GraphicsCommandList4> command_list,
    std::vector<BLASVertexInput> vertex_buffers) // sizeof(Vertex)
{
    nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;

    // Adding all vertex buffers and not transforming their position.
    for (const auto& buffer : vertex_buffers)
    {
        bottomLevelAS.AddVertexBuffer(buffer.vertex_buffer_view, buffer.index_buffer_view, buffer.vertex_count, buffer.index_count, 0, 0);
    }

    // The AS build requires some scratch space to store temporary information.
    // The amount of scratch memory is dependent on the scene complexity.
    UINT64 scratch_size_in_bytes = 0;

    // The final AS also needs to be stored in addition to the existing vertex
    // buffers. It size is also dependent on the scene complexity.
    UINT64 result_size_in_bytes = 0;

    bottomLevelAS.ComputeASBufferSizes(device.get(), false, &scratch_size_in_bytes, &result_size_in_bytes);

    // Once the sizes are obtained, the application is responsible for allocating
    // the necessary buffers. Since the entire generation will be done on the GPU,
    // we can directly allocate those on the default heap
    AccelerationStructureBuffers buffers;
    buffers.scratch = CreateBuffer(
        device.get(), scratch_size_in_bytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, nv_helpers_dx12::kDefaultHeapProps);

    buffers.result = CreateBuffer(
        device.get(),
        result_size_in_bytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        nv_helpers_dx12::kDefaultHeapProps);

    // Build the acceleration structure. Note that this call integrates a barrier
    // on the generated AS, so that it can be used to compute a top-level AS right
    // after this method.
    bottomLevelAS.Generate(command_list.get(), buffers.scratch.get(), buffers.result.get(), false, nullptr);

    return buffers;
}

//-----------------------------------------------------------------------------
// Create the main acceleration structure that holds all instances of the scene.
// Similarly to the bottom-level AS generation, it is done in 3 steps: gathering
// the instances, computing the memory requirements for the AS, and building the
// AS itself
//
// Instances are pair of bottom level AS and matrix of the instance
void ysn::RaytracingContext::CreateTopLevelAS(
    wil::com_ptr<ID3D12Device5> device, wil::com_ptr<ID3D12GraphicsCommandList4> command_list, const std::vector<TlasInput>& input_instances)
{
    // Gather all the instances into the builder helper
    for (size_t i = 0; i < input_instances.size(); i++)
    {
        tlas_generator.AddInstance(
            input_instances[i].blas.get(), input_instances[i].transform, input_instances[i].instance_id, static_cast<uint32_t>(0));
    }

    // As for the bottom-level AS, the building the AS requires some scratch space
    // to store temporary data in addition to the actual AS. In the case of the
    // top-level AS, the instance descriptors also need to be stored in GPU
    // memory. This call outputs the memory requirements for each (scratch,
    // results, instance descriptors) so that the application can allocate the
    // corresponding memory
    UINT64 scratchSize, resultSize, instanceDescsSize;

    tlas_generator.ComputeASBufferSizes(device.get(), true, &scratchSize, &resultSize, &instanceDescsSize);

    // Create the scratch and result buffers. Since the build is all done on GPU,
    // those can be allocated on the default heap
    tlas_buffers.scratch = CreateBuffer(
        device.get(), scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nv_helpers_dx12::kDefaultHeapProps);

    tlas_buffers.result = CreateBuffer(
        device.get(),
        resultSize,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        nv_helpers_dx12::kDefaultHeapProps);

    // The buffer describing the instances: ID, shader binding information,
    // matrices ... Those will be copied into the buffer by the helper through
    // mapping, so the buffer has to be allocated on the upload heap.
    tlas_buffers.instance_desc = CreateBuffer(
        device.get(), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

    // After all the buffers are allocated, or if only an update is required, we
    // can build the acceleration structure. Note that in the case of the update
    // we also pass the existing AS as the 'previous' AS, so that it can be
    // refitted in place.
    tlas_generator.Generate(command_list.get(), tlas_buffers.scratch.get(), tlas_buffers.result.get(), tlas_buffers.instance_desc.get());
}

void ysn::RaytracingContext::CreateTlasSrv(std::shared_ptr<ysn::DxRenderer> renderer)
{
    tlas_buffers.tlas_srv = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.RaytracingAccelerationStructure.Location = tlas_buffers.result->GetGPUVirtualAddress();

    // Write the acceleration structure view in the heap
    renderer->GetDevice()->CreateShaderResourceView(nullptr, &srv_desc, tlas_buffers.tlas_srv.cpu);
}

// Combine the BLAS and TLAS builds to construct the entire acceleration structure required to raytrace the scene

void ysn::RaytracingContext::CreateAccelerationStructures(wil::com_ptr<ID3D12GraphicsCommandList4> command_list, const RenderScene& render_scene)
{
    std::shared_ptr<DxRenderer> renderer = Application::Get().GetRenderer();

    for (auto& model : render_scene.models)
    {
        for (int i = 0; i < model.meshes.size(); i++)
        {
            const ysn::Mesh& mesh = model.meshes[i];
            const ysn::NodeTransform& transform = model.transforms[i];

            for (auto& primitive : mesh.primitives)
            {
                std::vector<BLASVertexInput> vertex_buffers;

                BLASVertexInput blas_input;

                blas_input.vertex_count = primitive.vertex_count;
                blas_input.index_count = primitive.index_count;
                blas_input.vertex_buffer_view = primitive.vertex_buffer_view;
                blas_input.index_buffer_view = primitive.index_buffer_view;

                vertex_buffers.push_back(blas_input);

                // Build the bottom AS from the Triangle vertex buffer
                AccelerationStructureBuffers bottomLevelBuffers =
                    CreateBottomLevelAS(renderer->GetDevice(), command_list, vertex_buffers);

                TlasInput tlas_input;
                tlas_input.blas = bottomLevelBuffers.result;
                tlas_input.transform = transform.transform;
                tlas_input.instance_id = primitive.index;

                instances.emplace_back(tlas_input);

                blas_res.push_back(bottomLevelBuffers.result);

#ifndef YSN_RELEASE
                bottomLevelBuffers.result->SetName(L"BLAS");
#endif
            }
        }
    }

    CreateTopLevelAS(renderer->GetDevice(), command_list, instances);

#ifndef YSN_RELEASE
    tlas_buffers.result->SetName(L"TLAS");
#endif
}
