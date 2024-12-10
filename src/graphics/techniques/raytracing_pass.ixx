module;
// https://developer.nvidia.com/rtx/raytracing/dxr/DX12-Raytracing-tutorial-Part-2

#include <wil/com.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxcapi.h>
#include <DirectXMath.h>

#include <shader_structs.h>

export module graphics.techniques.raytracing_pass;

import std;
import yasno.camera;
import renderer.dxrenderer;
import renderer.raytracing_context;
import renderer.descriptor_heap;
import renderer.nv.rt_pipeline_generator;
import renderer.nv.rs_generator;
import renderer.nv.sbt_generator;
import renderer.dxr_helper;
import system.filesystem;
import system.logger;
import system.asserts;

export namespace ysn
{
class RaytracingPass
{
public:
    bool Initialize(
        std::shared_ptr<ysn::DxRenderer> renderer,
        wil::com_ptr<ID3D12Resource> scene_color,
        wil::com_ptr<ID3D12Resource> accumulation_buffer_color,
        RaytracingContext& rtx_context,
        wil::com_ptr<ID3D12Resource> camera_buffer,
        wil::com_ptr<ID3D12Resource> vertex_buffer,
        wil::com_ptr<ID3D12Resource> index_buffer,
        wil::com_ptr<ID3D12Resource> material_buffer,
        wil::com_ptr<ID3D12Resource> per_instance_buffer,
        uint32_t vertices_count,
        uint32_t indices_count,
        uint32_t materials_count,
        uint32_t primitives_count);

    bool CreateRaytracingPipeline(std::shared_ptr<ysn::DxRenderer> renderer);
    bool CreateShaderBindingTable(
        std::shared_ptr<ysn::DxRenderer> renderer,
        wil::com_ptr<ID3D12Resource> scene_color,
        wil::com_ptr<ID3D12Resource> accumulation_buffer_color,
        RaytracingContext& rtx_context,
        wil::com_ptr<ID3D12Resource> camera_buffer,
        wil::com_ptr<ID3D12Resource> vertex_buffer,
        wil::com_ptr<ID3D12Resource> index_buffer,
        wil::com_ptr<ID3D12Resource> material_buffer,
        wil::com_ptr<ID3D12Resource> per_instance_buffer,
        uint32_t vertices_count,
        uint32_t indices_count,
        uint32_t materials_count,
        uint32_t primitives_count);

    void Execute(
        std::shared_ptr<ysn::DxRenderer> renderer,
        wil::com_ptr<ID3D12GraphicsCommandList4> command_list,
        uint32_t width,
        uint32_t height,
        wil::com_ptr<ID3D12Resource> scene_color,
        wil::com_ptr<ID3D12Resource> camera_buffer);

    wil::com_ptr<ID3D12RootSignature> CreateRayGenSignature(std::shared_ptr<ysn::DxRenderer> renderer);
    wil::com_ptr<ID3D12RootSignature> CreateMissSignature(std::shared_ptr<ysn::DxRenderer> renderer);
    wil::com_ptr<ID3D12RootSignature> CreateHitSignature(std::shared_ptr<ysn::DxRenderer> renderer);

    wil::com_ptr<IDxcBlob> m_ray_gen_library;
    wil::com_ptr<IDxcBlob> m_hit_library;
    wil::com_ptr<IDxcBlob> m_miss_library;

    wil::com_ptr<ID3D12RootSignature> m_ray_gen_signature;
    wil::com_ptr<ID3D12RootSignature> m_hit_signature;
    wil::com_ptr<ID3D12RootSignature> m_miss_signature;

    wil::com_ptr<ID3D12Resource> m_sbt_storage;

    // Ray tracing pipeline state
    wil::com_ptr<ID3D12StateObject> m_rt_state_object;

    // Ray tracing pipeline state properties, retaining the shader identifiers
    // to use in the Shader Binding Table
    wil::com_ptr<ID3D12StateObjectProperties> m_rt_state_object_props;

    nv_helpers_dx12::ShaderBindingTableGenerator m_sbt_helper;

    std::vector<D3D12_STATIC_SAMPLER_DESC> m_static_samplers;
};
} // namespace ysn

module :private;

namespace ysn
{
struct HitInfo
{
    DirectX::XMFLOAT4 encoded_normals; // Octahedron encoded
    DirectX::XMFLOAT3 hit_position;
    uint32_t material_id;
    DirectX::XMFLOAT2 uvs;
};

// The ray generation shader needs to access 2 resources: the raytracing output
// and the top-level acceleration structure
wil::com_ptr<ID3D12RootSignature> RaytracingPass::CreateRayGenSignature(std::shared_ptr<ysn::DxRenderer> renderer)
{
    nv_helpers_dx12::RootSignatureGenerator rsc;

    rsc.AddHeapRangesParameter(
        {{0 /*u0*/,
          1 /*1 descriptor */,
          0 /*use the implicit register space 0*/,
          D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/,
          0 /*heap slot where the UAV is defined*/},
         {1 /*u1*/,
          1 /*1 descriptor */,
          0 /*use the implicit register space 0*/,
          D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/,
          1 /*heap slot where the UAV is defined*/},
         {0 /*t0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2}, // TLAS
         {1 /*t1*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3}, // VertexBuffer
         {2 /*t2*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4}, // IndexBuffer
         {3 /*t3*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5}, // MaterialBuffer
         {4 /*t4*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6}, // PerInstanceBuffer
         {0 /*b0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV /*Camera parameters*/, 7}});

    return rsc.Generate(renderer->GetDevice().get(), true, m_static_samplers);
}

// The hit shader communicates only through the ray payload, and therefore does
// not require any resources
wil::com_ptr<ID3D12RootSignature> RaytracingPass::CreateHitSignature(std::shared_ptr<ysn::DxRenderer> renderer)
{
    nv_helpers_dx12::RootSignatureGenerator rsc;

    rsc.AddHeapRangesParameter(
        {{0 /*u0*/,
          1 /*1 descriptor */,
          0 /*use the implicit register space 0*/,
          D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/,
          0 /*heap slot where the UAV is defined*/},
         {1 /*u1*/,
          1 /*1 descriptor */,
          0 /*use the implicit register space 0*/,
          D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/,
          1 /*heap slot where the UAV is defined*/},
         {0 /*t0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2}, // TLAS
         {1 /*t1*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3}, // VertexBuffer
         {2 /*t2*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4}, // IndexBuffer
         {3 /*t3*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5}, // MaterialBuffer
         {4 /*t4*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6}, // PerInstanceBuffer
         {0 /*b0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV /*Camera parameters*/, 7}});

    return rsc.Generate(renderer->GetDevice().get(), true, m_static_samplers);
}

// The miss shader communicates only through the ray payload, and therefore
// does not require any resources
wil::com_ptr<ID3D12RootSignature> RaytracingPass::CreateMissSignature(std::shared_ptr<ysn::DxRenderer> renderer)
{
    nv_helpers_dx12::RootSignatureGenerator rsc;

    rsc.AddHeapRangesParameter(
        {{0 /*u0*/,
          1 /*1 descriptor */,
          0 /*use the implicit register space 0*/,
          D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/,
          0 /*heap slot where the UAV is defined*/},
         {1 /*u1*/,
          1 /*1 descriptor */,
          0 /*use the implicit register space 0*/,
          D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/,
          1 /*heap slot where the UAV is defined*/},
         {0 /*t0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2}, // TLAS
         {1 /*t1*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3}, // VertexBuffer
         {2 /*t2*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4}, // IndexBuffer
         {3 /*t3*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5}, // MaterialBuffer
         {4 /*t4*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6}, // PerInstanceBuffer
         {0 /*b0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV /*Camera parameters*/, 7}});

    return rsc.Generate(renderer->GetDevice().get(), true, m_static_samplers);
}

bool RaytracingPass::Initialize(
    std::shared_ptr<ysn::DxRenderer> renderer,
    wil::com_ptr<ID3D12Resource> scene_color,
    wil::com_ptr<ID3D12Resource> accumulation_buffer_color,
    RaytracingContext& rtx_context,
    wil::com_ptr<ID3D12Resource> camera_buffer,
    wil::com_ptr<ID3D12Resource> vertex_buffer,
    wil::com_ptr<ID3D12Resource> index_buffer,
    wil::com_ptr<ID3D12Resource> material_buffer,
    wil::com_ptr<ID3D12Resource> per_instance_buffer,
    uint32_t vertices_count,
    uint32_t indices_count,
    uint32_t materials_count,
    uint32_t primitives_count)
{

    /*m_static_samplers.push_back(static_sampler);*/

    if (!CreateRaytracingPipeline(renderer))
    {
        LogError << "Can't create raytracing pass pipeline\n";
        return false;
    }

    if (!CreateShaderBindingTable(
            renderer,
            scene_color,
            accumulation_buffer_color,
            rtx_context,
            camera_buffer,
            vertex_buffer,
            index_buffer,
            material_buffer,
            per_instance_buffer,
            vertices_count,
            indices_count,
            materials_count,
            primitives_count))
    {
        LogError << "Can't create raytracing pass shader binding table\n";
        return false;
    }

    return true;
}

// The raytracing pipeline binds the shader code, root signatures and pipeline
// characteristics in a single structure used by DXR to invoke the shaders and
// manage temporary memory during raytracing
bool RaytracingPass::CreateRaytracingPipeline(std::shared_ptr<ysn::DxRenderer> renderer)
{
    nv_helpers_dx12::RayTracingPipelineGenerator pipeline(renderer->GetDevice().get());

    // The pipeline contains the DXIL code of all the shaders potentially executed
    // during the raytracing process. This section compiles the HLSL code into a
    // set of DXIL libraries. We chose to separate the code in several libraries
    // by semantic (ray generation, hit, miss) for clarity. Any code layout can be
    // used.

    ShaderCompileParameters ray_gen_parameters;
    ray_gen_parameters.shader_type = ShaderType::Library;
    ray_gen_parameters.shader_path = GetVirtualFilesystemPath(L"shaders/ray_gen.rt.hlsl");

    const auto ray_gen_result = renderer->GetShaderStorage()->CompileShader(ray_gen_parameters);

    if (!ray_gen_result.has_value())
    {
        LogError << "Can't compile ray gen shader\n";
        return false;
    }

    ShaderCompileParameters miss_parameters;
    miss_parameters.shader_type = ShaderType::Library;
    miss_parameters.shader_path = GetVirtualFilesystemPath(L"shaders/miss.rt.hlsl");

    const auto miss_result = renderer->GetShaderStorage()->CompileShader(miss_parameters);

    if (!miss_result.has_value())
    {
        LogError << "Can't compile miss shader\n";
        return false;
    }

    ShaderCompileParameters hit_parameters;
    hit_parameters.shader_type = ShaderType::Library;
    hit_parameters.shader_path = GetVirtualFilesystemPath(L"shaders/hit.rt.hlsl");

    const auto hit_result = renderer->GetShaderStorage()->CompileShader(hit_parameters);

    if (!hit_result.has_value())
    {
        LogError << "Can't compile hit shader\n";
        return false;
    }

    m_ray_gen_library = ray_gen_result.value();
    m_miss_library = miss_result.value();
    m_hit_library = hit_result.value();

    // In a way similar to DLLs, each library is associated with a number of
    // exported symbols. This
    // has to be done explicitly in the lines below. Note that a single library
    // can contain an arbitrary number of symbols, whose semantic is given in HLSL
    // using the [shader("xxx")] syntax
    pipeline.AddLibrary(m_ray_gen_library.get(), {L"RayGen"});
    pipeline.AddLibrary(m_miss_library.get(), {L"Miss"});
    pipeline.AddLibrary(m_hit_library.get(), {L"ClosestHit"});

    // To be used, each DX12 shader needs a root signature defining which
    // parameters and buffers will be accessed.
    m_ray_gen_signature = CreateRayGenSignature(renderer);
    m_miss_signature = CreateMissSignature(renderer);
    m_hit_signature = CreateHitSignature(renderer);

    // 3 different shaders can be invoked to obtain an intersection: an
    // intersection shader is called
    // when hitting the bounding box of non-triangular geometry. This is beyond
    // the scope of this tutorial. An any-hit shader is called on potential
    // intersections. This shader can, for example, perform alpha-testing and
    // discard some intersections. Finally, the closest-hit program is invoked on
    // the intersection point closest to the ray origin. Those 3 shaders are bound
    // together into a hit group.

    // Note that for triangular geometry the intersection shader is built-in. An
    // empty any-hit shader is also defined by default, so in our simple case each
    // hit group contains only the closest hit shader. Note that since the
    // exported symbols are defined above the shaders can be simply referred to by
    // name.

    // Hit group for the triangles, with a shader simply interpolating vertex colors
    pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");

    // The following section associates the root signature to each shader. Note
    // that we can explicitly show that some shaders share the same root signature
    // (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
    // to as hit groups, meaning that the underlying intersection, any-hit and
    // closest-hit shaders share the same root signature.
    pipeline.AddRootSignatureAssociation(m_ray_gen_signature.get(), {L"RayGen"});
    pipeline.AddRootSignatureAssociation(m_miss_signature.get(), {L"Miss"});
    pipeline.AddRootSignatureAssociation(m_hit_signature.get(), {L"HitGroup"});

    // The payload size defines the maximum size of the data carried by the rays,
    // ie. the the data
    // exchanged between shaders, such as the HitInfo structure in the HLSL code.
    // It is important to keep this value as low as possible as a too high value
    // would result in unnecessary memory consumption and cache trashing.
    pipeline.SetMaxPayloadSize(sizeof(HitInfo));

    // Upon hitting a surface, DXR can provide several attributes to the hit. In
    // our sample we just use the barycentric coordinates defined by the weights
    // u,v of the last two vertices of the triangle. The actual barycentrics can
    // be obtained using float3 barycentrics = float3(1.f-u-v, u, v);
    pipeline.SetMaxAttributeSize(2 * sizeof(float)); // barycentric coordinates

    // The raytracing process can shoot rays from existing hit points, resulting
    // in nested TraceRay calls. Our sample code traces only primary rays, which
    // then requires a trace depth of 1. Note that this recursion depth should be
    // kept to a minimum for best performance. Path tracing algorithms can be
    // easily flattened into a simple loop in the ray generation.
    pipeline.SetMaxRecursionDepth(1);

    // Compile the pipeline for execution on the GPU
    m_rt_state_object = pipeline.Generate();

    // Cast the state object into a properties object, allowing to later access the shader pointers by name
    if (auto result = m_rt_state_object->QueryInterface(IID_PPV_ARGS(&m_rt_state_object_props)); FAILED(result))
    {
        LogError << "Can't QueryInterface rt_state_object_props from CreateRaytracingPipeline\n";
        return false;
    }

    return true;
}

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

bool RaytracingPass::CreateShaderBindingTable(
    std::shared_ptr<ysn::DxRenderer> renderer,
    wil::com_ptr<ID3D12Resource> scene_color,
    wil::com_ptr<ID3D12Resource> accumulation_buffer_color,
    RaytracingContext& rtx_context,
    wil::com_ptr<ID3D12Resource> camera_buffer,
    wil::com_ptr<ID3D12Resource> vertex_buffer,
    wil::com_ptr<ID3D12Resource> index_buffer,
    wil::com_ptr<ID3D12Resource> material_buffer,
    wil::com_ptr<ID3D12Resource> per_instance_buffer,
    uint32_t vertices_count,
    uint32_t indices_count,
    uint32_t materials_count,
    uint32_t primitives_count)
{
    // The SBT helper class collects calls to Add*Program.  If called several
    // times, the helper must be emptied before re-adding shaders.
    m_sbt_helper.Reset();

    // The pointer to the beginning of the heap is the only parameter required by shaders without root parameters
    // const auto srvUavHeapHandle = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

    // 1. UAV again scene color
    const auto scene_color_uav_handle = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // TODO: reuse hdr_format
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        renderer->GetDevice()->CreateUnorderedAccessView(scene_color.get(), nullptr, &uavDesc, scene_color_uav_handle.cpu);
    }

    // 2. UAV accumulation scene color
    {
        const auto accumulation_color_uav_handle = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // TODO: reuse hdr_format
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        renderer->GetDevice()->CreateUnorderedAccessView(
            accumulation_buffer_color.get(), nullptr, &uavDesc, accumulation_color_uav_handle.cpu);
    }

    // 3. SRV - TLAS
    rtx_context.CreateTlasSrv(renderer);

    // 4. SRV - Vertex Buffer
    {
        // Create SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Buffer.FirstElement = 0;
        srv_desc.Buffer.NumElements = static_cast<UINT>(vertices_count);
        srv_desc.Buffer.StructureByteStride = sizeof(Vertex);
        srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        auto vertices_buffer_srv = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
        renderer->GetDevice()->CreateShaderResourceView(vertex_buffer.get(), &srv_desc, vertices_buffer_srv.cpu);
    }

    // 5. SRV - Index Buffer
    {
        // Create SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Buffer.FirstElement = 0;
        srv_desc.Buffer.NumElements = static_cast<UINT>(indices_count);
        srv_desc.Buffer.StructureByteStride = sizeof(uint32_t);
        srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        auto indices_buffer_srv = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
        renderer->GetDevice()->CreateShaderResourceView(index_buffer.get(), &srv_desc, indices_buffer_srv.cpu);
    }

    // 6. SRV - Material Buffer
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Buffer.FirstElement = 0;
        srv_desc.Buffer.NumElements = static_cast<UINT>(materials_count);
        srv_desc.Buffer.StructureByteStride = sizeof(SurfaceShaderParameters);
        srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        const auto materials_buffer_srv = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
        renderer->GetDevice()->CreateShaderResourceView(material_buffer.get(), &srv_desc, materials_buffer_srv.cpu);
    }

    // 7. SRV - Per Instance Data buffer
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Buffer.FirstElement = 0;
        srv_desc.Buffer.NumElements = static_cast<UINT>(primitives_count);
        srv_desc.Buffer.StructureByteStride = sizeof(RenderInstanceData);
        srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        const auto per_instance_data_buffer_srv = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
        renderer->GetDevice()->CreateShaderResourceView(per_instance_buffer.get(), &srv_desc, per_instance_data_buffer_srv.cpu);
    }

    // 8. SRV - Camera
    {
        const auto camera_srv_handle = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
        cbv_desc.BufferLocation = camera_buffer->GetGPUVirtualAddress();
        cbv_desc.SizeInBytes = GetGpuSize<GpuCamera>();

        renderer->GetDevice()->CreateConstantBufferView(&cbv_desc, camera_srv_handle.cpu);
    }

    // The helper treats both root parameter pointers and heap pointers as void*,
    // while DX12 uses the
    // D3D12_GPU_DESCRIPTOR_HANDLE to define heap pointers. The pointer in this
    // struct is a UINT64, which then has to be reinterpreted as a pointer.
    auto heap_pointer = reinterpret_cast<void*>(scene_color_uav_handle.gpu.ptr); // TODO(rtx)
    // auto tlas_pointer = reinterpret_cast<void*>(rtx_context.tlas_buffers.tlas_srv.gpu.ptr); // TODO(rtx)

    // The ray generation only uses heap data
    m_sbt_helper.AddRayGenerationProgram(L"RayGen", {heap_pointer}); // TODO(rtx): heap_pointer

    // The miss and hit shaders do not access any external resources: instead they
    // communicate their results through the ray payload
    m_sbt_helper.AddMissProgram(L"Miss", {heap_pointer});

    // Adding the triangle hit shader
    m_sbt_helper.AddHitGroup(L"HitGroup", {heap_pointer});

    // Compute the size of the SBT given the number of shaders and their
    // parameters
    uint32_t sbtSize = m_sbt_helper.ComputeSBTSize();

    // Create the SBT on the upload heap. This is required as the helper will use
    // mapping to write the SBT contents. After the SBT compilation it could be
    // copied to the default heap for performance.
    m_sbt_storage = CreateBuffer(
        renderer->GetDevice().get(), sbtSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
    if (!m_sbt_storage)
    {
        LogError << "Could not allocate the shader binding table\n";
        return false;
    }

    // Compile the SBT from the shader and parameters info
    m_sbt_helper.Generate(m_sbt_storage.get(), m_rt_state_object_props.get());

    return true;
}

void RaytracingPass::Execute(
    std::shared_ptr<ysn::DxRenderer> renderer,
    wil::com_ptr<ID3D12GraphicsCommandList4> command_list,
    uint32_t width,
    uint32_t height,
    wil::com_ptr<ID3D12Resource> scene_color,
    wil::com_ptr<ID3D12Resource> camera_buffer)
{
    ID3D12DescriptorHeap* ppHeaps[] = {renderer->GetCbvSrvUavDescriptorHeap()->GetHeapPtr()};
    command_list->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // On the last frame, the raytracing output was used as a copy source, to
    // copy its contents into the render target. Now we need to transition it to
    // a UAV so that the shaders can write in it.
    CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
        scene_color.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    command_list->ResourceBarrier(1, &transition);

    // Setup the raytracing task
    D3D12_DISPATCH_RAYS_DESC desc = {};
    // The layout of the SBT is as follows: ray generation shader, miss
    // shaders, hit groups. As described in the CreateShaderBindingTable method,
    // all SBT entries of a given type have the same size to allow a fixed stride.

    // The ray generation shaders are always at the beginning of the SBT.
    uint32_t rayGenerationSectionSizeInBytes = m_sbt_helper.GetRayGenSectionSize();
    desc.RayGenerationShaderRecord.StartAddress = m_sbt_storage->GetGPUVirtualAddress();
    desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSectionSizeInBytes;

    // The miss shaders are in the second SBT section, right after the ray
    // generation shader. We have one miss shader for the camera rays and one
    // for the shadow rays, so this section has a size of 2*m_sbtEntrySize. We
    // also indicate the stride between the two miss shaders, which is the size
    // of a SBT entry
    uint32_t missSectionSizeInBytes = m_sbt_helper.GetMissSectionSize();
    desc.MissShaderTable.StartAddress = m_sbt_storage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes;
    desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
    desc.MissShaderTable.StrideInBytes = m_sbt_helper.GetMissEntrySize();

    // The hit groups section start after the miss shaders. In this sample we
    // have one 1 hit group for the triangle
    uint32_t hitGroupsSectionSize = m_sbt_helper.GetHitGroupSectionSize();
    desc.HitGroupTable.StartAddress = m_sbt_storage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes + missSectionSizeInBytes;
    desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
    desc.HitGroupTable.StrideInBytes = m_sbt_helper.GetHitGroupEntrySize();

    // Dimensions of the image to render, identical to a kernel launch dimension
    desc.Width = width;
    desc.Height = height;
    desc.Depth = 1;

    // Bind the raytracing pipeline
    command_list->SetPipelineState1(m_rt_state_object.get());

    // Dispatch the rays and write to the raytracing output
    command_list->DispatchRays(&desc);

    // The raytracing output needs to be copied to the actual render target used
    // for display. For this, we need to transition the raytracing output from a
    // UAV to a copy source, and the render target buffer to a copy destination.
    // We can then do the actual copy, before transitioning the render target
    // buffer into a render target, that will be then used to display the image
    transition = CD3DX12_RESOURCE_BARRIER::Transition(
        scene_color.get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RENDER_TARGET); // TODO(rtx): This transition is done in Tonemap, so it's duplicatr

    command_list->ResourceBarrier(1, &transition);
}
} // namespace ysn
