module;

#include <wil/com.h>
#include <d3d12.h>
#include <DirectXMath.h>

export module renderer.rtx_context;

import std;
import system.application;
import system.gltf_loader;
import graphics.render_scene;
import renderer.dxrenderer;
import renderer.descriptor_heap;
import renderer.tlas_generator;
import renderer.blas_generator;
import renderer.dx_types;
import renderer.gpu_buffer;

export namespace ysn
{
struct AccelerationStructureBuffers
{
    GpuBuffer scratch;       
    GpuBuffer result;        
    GpuBuffer instance_desc;
    DescriptorHandle tlas_srv;
};

struct BlasInput
{
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
    D3D12_INDEX_BUFFER_VIEW index_buffer_view;
    uint32_t vertex_count;
    uint32_t index_count;
};

struct TlasInput
{
    GpuBuffer blas;
    DirectX::XMMATRIX transform;
    uint32_t instance_id = 0;
};

class RtxContext
{
public:
    AccelerationStructureBuffers CreateBlas(
        wil::com_ptr<DxDevice> device, wil::com_ptr<DxGraphicsCommandList> command_list, std::vector<BlasInput> vertex_buffers);

    void CreateTlas(wil::com_ptr<DxDevice> device, wil::com_ptr<DxGraphicsCommandList> command_list, const std::vector<TlasInput>& instances);
    void CreateTlasSrv(std::shared_ptr<ysn::DxRenderer> renderer);
    void CreateAccelerationStructures(wil::com_ptr<DxGraphicsCommandList> command_list, const RenderScene& render_scene);

    TlasGenerator tlas_generator;

    std::vector<AccelerationStructureBuffers> blas_res;

    AccelerationStructureBuffers tlas_buffers;
    std::vector<TlasInput> instances;
};
} // namespace ysn

module :private;

ysn::AccelerationStructureBuffers ysn::RtxContext::CreateBlas(
    wil::com_ptr<DxDevice> device,
    wil::com_ptr<DxGraphicsCommandList> command_list,
    std::vector<BlasInput> vertex_buffers)
{
    BlasGenerator blas_generator;

    for (const auto& buffer : vertex_buffers)
    {
        blas_generator.AddVertexBuffer(buffer.vertex_buffer_view, buffer.index_buffer_view, buffer.vertex_count, buffer.index_count, 0, 0);
    }

    // The AS build requires some scratch space to store temporary information.
    // The amount of scratch memory is dependent on the scene complexity.
    UINT64 scratch_size_in_bytes = 0;

    // The final AS also needs to be stored in addition to the existing vertex
    // buffers. It size is also dependent on the scene complexity.
    UINT64 result_size_in_bytes = 0;

    blas_generator.ComputeBlasBufferSizes(device.get(), false, &scratch_size_in_bytes, &result_size_in_bytes);

    GpuBufferCreateInfo scratch_create_info{
        .size = scratch_size_in_bytes,
        .heap_type = D3D12_HEAP_TYPE_DEFAULT,
        .flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        .state = D3D12_RESOURCE_STATE_COMMON,
    };

    GpuBufferCreateInfo blas_create_info{
        .size = result_size_in_bytes,
        .heap_type = D3D12_HEAP_TYPE_DEFAULT,
        .flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        .state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
    };

    const std::optional<GpuBuffer> scratch_result = CreateGpuBuffer(scratch_create_info, "Blas Scratch Buffer");
    const std::optional<GpuBuffer> blas_result = CreateGpuBuffer(blas_create_info, "Blas Buffer");

    AccelerationStructureBuffers buffers;
    buffers.scratch = scratch_result.value();
    buffers.result = blas_result.value();

    blas_generator.Generate(command_list.get(), buffers.scratch, buffers.result, false, nullptr);

    return buffers;
}

// Instances are pair of bottom level AS and matrix of the instance
void ysn::RtxContext::CreateTlas(
    wil::com_ptr<DxDevice> device, wil::com_ptr<DxGraphicsCommandList> command_list, const std::vector<TlasInput>& input_instances)
{
    // Gather all the instances into the builder helper
    for (size_t i = 0; i < input_instances.size(); i++)
    {
        tlas_generator.AddInstance(
            input_instances[i].blas, input_instances[i].transform, input_instances[i].instance_id, static_cast<uint32_t>(0));
    }

    // As for the bottom-level AS, the building the AS requires some scratch space
    // to store temporary data in addition to the actual AS. In the case of the
    // top-level AS, the instance descriptors also need to be stored in GPU
    // memory. This call outputs the memory requirements for each (scratch,
    // results, instance descriptors) so that the application can allocate the
    // corresponding memory
    UINT64 scratch_size, tlas_result_size, instance_buffer_size;

    tlas_generator.ComputeTlasBufferSizes(device.get(), true, &scratch_size, &tlas_result_size, &instance_buffer_size);

    GpuBufferCreateInfo scratch_create_info{
        .size = scratch_size,
        .heap_type = D3D12_HEAP_TYPE_DEFAULT,
        .flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        .state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
    };

    GpuBufferCreateInfo tlas_create_info{
        .size = tlas_result_size,
        .heap_type = D3D12_HEAP_TYPE_DEFAULT,
        .flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        .state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
    };

    GpuBufferCreateInfo instances_create_info{
        .size = instance_buffer_size,
        .heap_type = D3D12_HEAP_TYPE_UPLOAD,
        .flags = D3D12_RESOURCE_FLAG_NONE,
        .state = D3D12_RESOURCE_STATE_GENERIC_READ,
    };

    const std::optional<GpuBuffer> scratch_result = CreateGpuBuffer(scratch_create_info, "Tlas Scratch Buffer");
    const std::optional<GpuBuffer> tlas_result = CreateGpuBuffer(tlas_create_info, "Tlas Buffer");
    const std::optional<GpuBuffer> instances_result = CreateGpuBuffer(instances_create_info, "Tlas Instances Buffer");

    tlas_buffers.scratch = scratch_result.value();
    tlas_buffers.result = tlas_result.value();
    tlas_buffers.instance_desc = instances_result.value();

    // After all the buffers are allocated, or if only an update is required, we
    // can build the acceleration structure. Note that in the case of the update
    // we also pass the existing AS as the 'previous' AS, so that it can be
    // refitted in place.
    tlas_generator.Generate(command_list.get(), tlas_buffers.scratch, tlas_buffers.result, tlas_buffers.instance_desc);
}

void ysn::RtxContext::CreateTlasSrv(std::shared_ptr<ysn::DxRenderer> renderer)
{
    tlas_buffers.tlas_srv = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.RaytracingAccelerationStructure.Location = tlas_buffers.result.GPUVirtualAddress();

    // Write the acceleration structure view in the heap
    renderer->GetDevice()->CreateShaderResourceView(nullptr, &srv_desc, tlas_buffers.tlas_srv.cpu);
}

// Combine the BLAS and TLAS builds to construct the entire acceleration structure required to raytrace the scene

void ysn::RtxContext::CreateAccelerationStructures(wil::com_ptr<DxGraphicsCommandList> command_list, const RenderScene& render_scene)
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
                std::vector<BlasInput> vertex_buffers;

                BlasInput blas_input;

                blas_input.vertex_count = primitive.vertex_count;
                blas_input.index_count = primitive.index_count;
                blas_input.vertex_buffer_view = primitive.vertex_buffer_view;
                blas_input.index_buffer_view = primitive.index_buffer_view;

                vertex_buffers.push_back(blas_input);

                // Build the bottom AS from the Triangle vertex buffer
                AccelerationStructureBuffers blas_buffers = CreateBlas(renderer->GetDevice(), command_list, vertex_buffers);

                TlasInput tlas_input;
                tlas_input.blas = blas_buffers.result;
                tlas_input.transform = transform.transform;
                tlas_input.instance_id = primitive.index;

                instances.emplace_back(tlas_input);

                blas_res.push_back(blas_buffers);
            }
        }
    }

    CreateTlas(renderer->GetDevice(), command_list, instances);
}
