module;

#include <d3d12.h>

export module renderer.nv.blas_generator;

import std;
import renderer.dx_types;
import renderer.gpu_buffer;
import system.math;

export namespace nv_helpers_dx12
{
class BlasGenerator
{
public:
    void AddVertexBuffer(
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view,
        D3D12_INDEX_BUFFER_VIEW index_buffer_view,
        std::uint32_t vertexCount,
        std::uint32_t indexCount,
        ID3D12Resource* transformBuffer,
        UINT64 transformOffsetInBytes,
        bool isOpaque = true);

    void ComputeBlasBufferSizes(DxDevice* device, bool allowUpdate, UINT64* scratchSizeInBytes, UINT64* resultSizeInBytes);

    void Generate(
        DxGraphicsCommandList* commandList,
        const ysn::GpuBuffer& scratch_buffer,
        const ysn::GpuBuffer& result_buffer,
        bool updateOnly = false,
        ID3D12Resource* previousResult = nullptr);

private:
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_vertexBuffers = {};

    UINT64 m_scratchSizeInBytes = 0;
    UINT64 m_resultSizeInBytes = 0;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS m_flags;
};
} // namespace nv_helpers_dx12

module :private;

namespace nv_helpers_dx12
{
void BlasGenerator::AddVertexBuffer(
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view,
    D3D12_INDEX_BUFFER_VIEW index_buffer_view,
    std::uint32_t vertexCount,
    std::uint32_t indexCount,
    ID3D12Resource* transformBuffer,
    UINT64 transformOffsetInBytes,
    bool isOpaque)
{
    D3D12_RAYTRACING_GEOMETRY_DESC descriptor = {};
    descriptor.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    descriptor.Triangles.VertexBuffer.StartAddress = vertex_buffer_view.BufferLocation;
    descriptor.Triangles.VertexBuffer.StrideInBytes = vertex_buffer_view.StrideInBytes;
    descriptor.Triangles.VertexCount = vertexCount;
    descriptor.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    descriptor.Triangles.IndexBuffer = index_buffer_view.BufferLocation;
    descriptor.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    descriptor.Triangles.IndexCount = indexCount;
    descriptor.Triangles.Transform3x4 = transformBuffer ? (transformBuffer->GetGPUVirtualAddress() + transformOffsetInBytes) : 0;
    descriptor.Flags = isOpaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

    m_vertexBuffers.push_back(descriptor);
}

void BlasGenerator::ComputeBlasBufferSizes(
    DxDevice* device,
    bool allowUpdate,
    UINT64* scratchSizeInBytes,
    UINT64* resultSizeInBytes)
{
    m_flags = allowUpdate ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
                          : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS prebuildDesc;
    prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    prebuildDesc.NumDescs = static_cast<UINT>(m_vertexBuffers.size());
    prebuildDesc.pGeometryDescs = m_vertexBuffers.data();
    prebuildDesc.Flags = m_flags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

    *scratchSizeInBytes = ysn::AlignPow2(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    *resultSizeInBytes = ysn::AlignPow2(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    m_scratchSizeInBytes = *scratchSizeInBytes;
    m_resultSizeInBytes = *resultSizeInBytes;
}

void BlasGenerator::Generate(
    DxGraphicsCommandList* commandList, 
    const ysn::GpuBuffer& scratch_buffer,         
    const ysn::GpuBuffer& result_buffer,            
    bool updateOnly,                         
    ID3D12Resource* previousResult          
)
{
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = m_flags;

    if (flags == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && updateOnly)
    {
        flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    }

    if (m_flags != D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && updateOnly)
    {
        throw std::logic_error("Cannot update a bottom-level AS not originally built for updates");
    }

    if (updateOnly && previousResult == nullptr)
    {
        throw std::logic_error("Bottom-level hierarchy update requires the previous hierarchy");
    }

    if (m_resultSizeInBytes == 0 || m_scratchSizeInBytes == 0)
    {
        throw std::logic_error("Invalid scratch and result buffer sizes - ComputeBlasBufferSizes needs to be called before Build");
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc;
    buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildDesc.Inputs.NumDescs = static_cast<UINT>(m_vertexBuffers.size());
    buildDesc.Inputs.pGeometryDescs = m_vertexBuffers.data();
    buildDesc.DestAccelerationStructureData = {result_buffer.GetGPUVirtualAddress()};
    buildDesc.ScratchAccelerationStructureData = {scratch_buffer.GetGPUVirtualAddress()};
    buildDesc.SourceAccelerationStructureData = previousResult ? previousResult->GetGPUVirtualAddress() : 0;
    buildDesc.Inputs.Flags = flags;

    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    D3D12_RESOURCE_BARRIER uavBarrier;
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = result_buffer.resource.get();
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

    commandList->ResourceBarrier(1, &uavBarrier);
}
} // namespace nv_helpers_dx12
