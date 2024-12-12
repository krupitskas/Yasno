module;

#include <d3d12.h>
#include <DirectXMath.h>

export module renderer.nv.tlas_generator;

import std;
import system.math;
import renderer.gpu_buffer;

export namespace nv_helpers_dx12
{
class TlasGenerator
{
public:
    void AddInstance(const ysn::GpuBuffer& blas, 
        const DirectX::XMMATRIX& transform,
        UINT instanceID,
        UINT hitGroupIndex 
    );

    void ComputeTlasBufferSizes(
        ID3D12Device5* device,        
        bool allowUpdate,        
        UINT64* scratchSizeInBytes, 
        UINT64* resultSizeInBytes,   
        UINT64* descriptorsSizeInBytes 
    );

    void Generate(
        ID3D12GraphicsCommandList4* commandList, 
        const ysn::GpuBuffer& scratch_buffer,
        ysn::GpuBuffer& result_buffer,    
        const ysn::GpuBuffer& descriptorsBuffer,
        bool updateOnly = false,                 
        ID3D12Resource* previousResult = nullptr 
    );

private:
    struct Instance
    {
        Instance(const ysn::GpuBuffer& blas, const DirectX::XMMATRIX& tr, UINT iID, UINT hgId);

        ysn::GpuBuffer blas;
        const DirectX::XMMATRIX& transform;
        UINT instanceID;
        UINT hitGroupIndex;
    };

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS m_flags;
    std::vector<Instance> m_instances;
    UINT64 m_scratchSizeInBytes;
    UINT64 m_instanceDescsSizeInBytes;
    UINT64 m_resultSizeInBytes;
};
} // namespace nv_helpers_dx12

module :private;

namespace nv_helpers_dx12
{

void TlasGenerator::AddInstance(
    const ysn::GpuBuffer& blas, 
    const DirectX::XMMATRIX& transform,
    UINT instanceID, 
    UINT hitGroupIndex 
)
{
    m_instances.emplace_back(Instance(blas, transform, instanceID, hitGroupIndex));
}

void TlasGenerator::ComputeTlasBufferSizes(
    ID3D12Device5* device, 
    bool allowUpdate,
    UINT64* scratchSizeInBytes, 
    UINT64* resultSizeInBytes,
    UINT64* descriptorsSizeInBytes 
)
{
    m_flags = allowUpdate ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
                          : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS
    prebuildDesc = {};
    prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    prebuildDesc.NumDescs = static_cast<UINT>(m_instances.size());
    prebuildDesc.Flags = m_flags;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

    info.ResultDataMaxSizeInBytes = ysn::AlignPow2(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    info.ScratchDataSizeInBytes = ysn::AlignPow2(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    m_resultSizeInBytes = info.ResultDataMaxSizeInBytes;
    m_scratchSizeInBytes = info.ScratchDataSizeInBytes;
    m_instanceDescsSizeInBytes = ysn::AlignPow2(
        sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * static_cast<UINT64>(m_instances.size()), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    *scratchSizeInBytes = m_scratchSizeInBytes;
    *resultSizeInBytes = m_resultSizeInBytes;
    *descriptorsSizeInBytes = m_instanceDescsSizeInBytes;
}

void TlasGenerator::Generate(
    ID3D12GraphicsCommandList4* commandList, 
    const ysn::GpuBuffer& scratch_buffer,          
    ysn::GpuBuffer& result_buffer,      
    const ysn::GpuBuffer& descriptorsBuffer,
    bool updateOnly ,
    ID3D12Resource* previousResult 
)
{
    D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs;
    descriptorsBuffer.GetResourcePtr()->Map(0, nullptr, reinterpret_cast<void**>(&instanceDescs));
    if (!instanceDescs)
    {
        throw std::logic_error(
            "Cannot map the instance descriptor buffer - is it "
            "in the upload heap?");
    }

    auto instanceCount = static_cast<UINT>(m_instances.size());

    if (!updateOnly)
    {
        ZeroMemory(instanceDescs, m_instanceDescsSizeInBytes);
    }

    for (uint32_t i = 0; i < instanceCount; i++)
    {
        instanceDescs[i].InstanceID = m_instances[i].instanceID;
        instanceDescs[i].InstanceContributionToHitGroupIndex = m_instances[i].hitGroupIndex;
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
        throw std::logic_error("Cannot update a top-level AS not originally built for updates");
    }
    if (updateOnly && previousResult == nullptr)
    {
        throw std::logic_error("Top-level hierarchy update requires the previous hierarchy");
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

    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    D3D12_RESOURCE_BARRIER uavBarrier;
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = result_buffer.GetResourcePtr();
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    commandList->ResourceBarrier(1, &uavBarrier);

    result_buffer.SetName(L"Tlas Buffer");
}

TlasGenerator::Instance::Instance(const ysn::GpuBuffer& blas, const DirectX::XMMATRIX& tr, UINT iID, UINT hgId) :
    blas(blas), transform(tr), instanceID(iID), hitGroupIndex(hgId)
{
}
} // namespace nv_helpers_dx12
