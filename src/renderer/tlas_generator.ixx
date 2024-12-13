module;

#include <d3d12.h>
#include <DirectXMath.h>

export module renderer.tlas_generator;

import std;
import system.math;
import renderer.gpu_buffer;
import renderer.dx_types;
import system.logger;

export namespace ysn
{
class TlasGenerator
{
public:
    void AddInstance(const GpuBuffer& blas, const DirectX::XMMATRIX& transform, UINT instace_id, UINT hit_group_index);

    void ComputeTlasBufferSizes(
        DxDevice* device, bool allow_update, UINT64* scratch_size_in_bytes, UINT64* result_size_in_bytes, UINT64* descriptorsSizeInBytes);

    bool Generate(
        ID3D12GraphicsCommandList4* cmd_list,
        const GpuBuffer& scratch_buffer,
        GpuBuffer& result_buffer,
        const GpuBuffer& descriptorsBuffer,
        bool updateOnly = false,
        ID3D12Resource* previousResult = nullptr);
private:
    struct Instance
    {
        Instance(const GpuBuffer& blas, const DirectX::XMMATRIX& tr, UINT iID, UINT hgId);

        GpuBuffer blas;
        const DirectX::XMMATRIX& transform;
        UINT instace_id;
        UINT hit_group_index;
    };

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS m_flags;
    std::vector<Instance> m_instances;
    UINT64 m_scratch_size_in_bytes;
    UINT64 m_instance_descs_size_in_bytes;
    UINT64 m_result_size_in_bytes;
};
} // namespace ysn

module :private;

namespace ysn
{

void TlasGenerator::AddInstance(const GpuBuffer& blas, const DirectX::XMMATRIX& transform, UINT instace_id, UINT hit_group_index)
{
    m_instances.emplace_back(Instance(blas, transform, instace_id, hit_group_index));
}

void TlasGenerator::ComputeTlasBufferSizes(
    DxDevice* device, bool allow_update, UINT64* scratch_size_in_bytes, UINT64* result_size_in_bytes, UINT64* descriptorsSizeInBytes)
{
    m_flags = allow_update ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
                           : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS
    prebuildDesc = {};
    prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    prebuildDesc.NumDescs = static_cast<UINT>(m_instances.size());
    prebuildDesc.Flags = m_flags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

    info.ResultDataMaxSizeInBytes = AlignPow2(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    info.ScratchDataSizeInBytes = AlignPow2(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    m_result_size_in_bytes = info.ResultDataMaxSizeInBytes;
    m_scratch_size_in_bytes = info.ScratchDataSizeInBytes;
    m_instance_descs_size_in_bytes = AlignPow2(
        sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * static_cast<UINT64>(m_instances.size()), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    *scratch_size_in_bytes = m_scratch_size_in_bytes;
    *result_size_in_bytes = m_result_size_in_bytes;
    *descriptorsSizeInBytes = m_instance_descs_size_in_bytes;
}

bool TlasGenerator::Generate(
    ID3D12GraphicsCommandList4* cmd_list,
    const GpuBuffer& scratch_buffer,
    GpuBuffer& result_buffer,
    const GpuBuffer& descriptorsBuffer,
    bool updateOnly,
    ID3D12Resource* previousResult)
{
    D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs;
    descriptorsBuffer.GetResourcePtr()->Map(0, nullptr, reinterpret_cast<void**>(&instanceDescs));

    if (!instanceDescs)
    {
        LogFatal << "Cannot map the instance descriptor buffer - is it in the upload heap?\n";
        return false;
    }

    auto instanceCount = static_cast<UINT>(m_instances.size());

    if (!updateOnly)
    {
        ZeroMemory(instanceDescs, m_instance_descs_size_in_bytes);
    }

    for (uint32_t i = 0; i < instanceCount; i++)
    {
        instanceDescs[i].InstanceID = m_instances[i].instace_id;
        instanceDescs[i].InstanceContributionToHitGroupIndex = m_instances[i].hit_group_index;
        instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        DirectX::XMMATRIX m = XMMatrixTranspose(m_instances[i].transform);
        memcpy(instanceDescs[i].Transform, &m, sizeof(instanceDescs[i].Transform));
        instanceDescs[i].AccelerationStructure = m_instances[i].blas.GetGPUVirtualAddress();
        instanceDescs[i].InstanceMask = 0xFF;
    }

    descriptorsBuffer.GetResourcePtr()->Unmap(0, nullptr);

    D3D12_GPU_VIRTUAL_ADDRESS pSourceAS = updateOnly ? previousResult->GetGPUVirtualAddress() : 0;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = m_flags;
    if (flags == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && updateOnly)
    {
        flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    }

    if (m_flags != D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && updateOnly)
    {
        LogFatal << "Cannot update a top-level AS not originally built for updates\n";
        return false;
    }

    if (updateOnly && previousResult == nullptr)
    {
        LogFatal << "Top-level hierarchy update requires the previous hierarchy\n";
        return false;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildDesc.Inputs.InstanceDescs = descriptorsBuffer.GetGPUVirtualAddress();
    buildDesc.Inputs.NumDescs = instanceCount;
    buildDesc.DestAccelerationStructureData = result_buffer.GetGPUVirtualAddress();
    buildDesc.ScratchAccelerationStructureData = scratch_buffer.GetGPUVirtualAddress();
    buildDesc.SourceAccelerationStructureData = pSourceAS;
    buildDesc.Inputs.Flags = flags;

    cmd_list->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    D3D12_RESOURCE_BARRIER uavBarrier;
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = result_buffer.GetResourcePtr();
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    cmd_list->ResourceBarrier(1, &uavBarrier);

    result_buffer.SetName(L"TLAS buffer");

    return true;
}

TlasGenerator::Instance::Instance(const GpuBuffer& blas, const DirectX::XMMATRIX& tr, UINT iID, UINT hgId) :
    blas(blas), transform(tr), instace_id(iID), hit_group_index(hgId)
{
}
} // namespace ysn
