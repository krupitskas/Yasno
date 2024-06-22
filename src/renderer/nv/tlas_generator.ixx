module;

#include <d3d12.h>
#include <DirectXMath.h>

export module renderer.nv.tlas_generator;

import std;

export namespace nv_helpers_dx12
{
class TopLevelASGenerator
{
public:
    /// Add an instance to the top-level acceleration structure. The instance is
    /// represented by a bottom-level AS, a transform, an instance ID and the
    /// index of the hit group indicating which shaders are executed upon hitting
    /// any geometry within the instance
    void AddInstance(
        ID3D12Resource* bottomLevelAS, /// Bottom-level acceleration structure containing the
        /// actual geometric data of the instance
        const DirectX::XMMATRIX& transform, /// Transform matrix to apply to the instance,
        /// allowing the same bottom-level AS to be used
        /// at several world-space positions
        UINT instanceID, /// Instance ID, which can be used in the shaders to
        /// identify this specific instance
        UINT hitGroupIndex /// Hit group index, corresponding the the index of the
        /// hit group in the Shader Binding Table that will be
        /// invocated upon hitting the geometry
    );

    /// Compute the size of the scratch space required to build the acceleration
    /// structure, as well as the size of the resulting structure. The allocation
    /// of the buffers is then left to the application
    void ComputeASBufferSizes(
        ID3D12Device5* device,         /// Device on which the build will be performed
        bool allowUpdate,              /// If true, the resulting acceleration structure will allow iterative updates
        UINT64* scratchSizeInBytes,    /// Required scratch memory on the GPU to build the acceleration structure
        UINT64* resultSizeInBytes,     /// Required GPU memory to store the acceleration structure
        UINT64* descriptorsSizeInBytes /// Required GPU memory to store instance descriptors, containing the matrices indices etc.
    );

    /// Enqueue the construction of the acceleration structure on a command list,
    /// using application-provided buffers and possibly a pointer to the previous
    /// acceleration structure in case of iterative updates. Note that the update
    /// can be done in place: the result and previousResult pointers can be the
    /// same.
    void Generate(
        ID3D12GraphicsCommandList4* commandList, /// Command list on which the build will be enqueued
        ID3D12Resource* scratchBuffer,           /// Scratch buffer used by the builder to
        /// store temporary data
        ID3D12Resource* resultBuffer,      /// Result buffer storing the acceleration structure
        ID3D12Resource* descriptorsBuffer, /// Auxiliary result buffer containing the instance
        /// descriptors, has to be in upload heap
        bool updateOnly = false,                 /// If true, simply refit the existing acceleration structure
        ID3D12Resource* previousResult = nullptr /// Optional previous acceleration structure, used
        /// if an iterative update is requested
    );

private:
    /// Helper struct storing the instance data
    struct Instance
    {
        Instance(ID3D12Resource* blAS, const DirectX::XMMATRIX& tr, UINT iID, UINT hgId);
        /// Bottom-level AS
        ID3D12Resource* bottomLevelAS;
        /// Transform matrix
        const DirectX::XMMATRIX& transform;
        /// Instance ID visible in the shader
        UINT instanceID;
        /// Hit group index used to fetch the shaders from the SBT
        UINT hitGroupIndex;
    };

    /// Construction flags, indicating whether the AS supports iterative updates
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS m_flags;
    /// Instances contained in the top-level AS
    std::vector<Instance> m_instances;

    /// Size of the temporary memory used by the TLAS builder
    UINT64 m_scratchSizeInBytes;
    /// Size of the buffer containing the instance descriptors
    UINT64 m_instanceDescsSizeInBytes;
    /// Size of the buffer containing the TLAS
    UINT64 m_resultSizeInBytes;
};
} // namespace nv_helpers_dx12

module :private;

// Helper to compute aligned buffer sizes
#ifndef ROUND_UP
#define ROUND_UP(v, powerOf2Alignment) (((v) + (powerOf2Alignment) - 1) & ~((powerOf2Alignment) - 1))
#endif

namespace nv_helpers_dx12
{

//--------------------------------------------------------------------------------------------------
//
// Add an instance to the top-level acceleration structure. The instance is
// represented by a bottom-level AS, a transform, an instance ID and the index
// of the hit group indicating which shaders are executed upon hitting any
// geometry within the instance
void TopLevelASGenerator::AddInstance(
    ID3D12Resource* bottomLevelAS, // Bottom-level acceleration structure containing the
    // actual geometric data of the instance
    const DirectX::XMMATRIX& transform, // Transform matrix to apply to the instance, allowing the
    // same bottom-level AS to be used at several world-space
    // positions
    UINT instanceID, // Instance ID, which can be used in the shaders to
    // identify this specific instance
    UINT hitGroupIndex // Hit group index, corresponding the the index of the
    // hit group in the Shader Binding Table that will be
    // invocated upon hitting the geometry
)
{
    m_instances.emplace_back(Instance(bottomLevelAS, transform, instanceID, hitGroupIndex));
}

//--------------------------------------------------------------------------------------------------
//
// Compute the size of the scratch space required to build the acceleration
// structure, as well as the size of the resulting structure. The allocation of
// the buffers is then left to the application
void TopLevelASGenerator::ComputeASBufferSizes(
    ID3D12Device5* device, // Device on which the build will be performed
    bool allowUpdate,      // If true, the resulting acceleration structure will
    // allow iterative updates
    UINT64* scratchSizeInBytes, // Required scratch memory on the GPU to build
    // the acceleration structure
    UINT64* resultSizeInBytes, // Required GPU memory to store the acceleration
    // structure
    UINT64* descriptorsSizeInBytes // Required GPU memory to store instance
    // descriptors, containing the matrices,
    // indices etc.
)
{
    // The generated AS can support iterative updates. This may change the final
    // size of the AS as well as the temporary memory requirements, and hence has
    // to be set before the actual build
    m_flags = allowUpdate ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
                          : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

    // Describe the work being requested, in this case the construction of a
    // (possibly dynamic) top-level hierarchy, with the given instance descriptors
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS
    prebuildDesc = {};
    prebuildDesc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    prebuildDesc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    prebuildDesc.NumDescs = static_cast<UINT>(m_instances.size());
    prebuildDesc.Flags = m_flags;

    // This structure is used to hold the sizes of the required scratch memory and
    // resulting AS
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};

    // Building the acceleration structure (AS) requires some scratch space, as
    // well as space to store the resulting structure This function computes a
    // conservative estimate of the memory requirements for both, based on the
    // number of bottom-level instances.
    device->GetRaytracingAccelerationStructurePrebuildInfo(&prebuildDesc, &info);

    // Buffer sizes need to be 256-byte-aligned
    info.ResultDataMaxSizeInBytes = ROUND_UP(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    info.ScratchDataSizeInBytes = ROUND_UP(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    m_resultSizeInBytes = info.ResultDataMaxSizeInBytes;
    m_scratchSizeInBytes = info.ScratchDataSizeInBytes;
    // The instance descriptors are stored as-is in GPU memory, so we can deduce
    // the required size from the instance count
    m_instanceDescsSizeInBytes = ROUND_UP(
        sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * static_cast<UINT64>(m_instances.size()), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    *scratchSizeInBytes = m_scratchSizeInBytes;
    *resultSizeInBytes = m_resultSizeInBytes;
    *descriptorsSizeInBytes = m_instanceDescsSizeInBytes;
}

//--------------------------------------------------------------------------------------------------
//
// Enqueue the construction of the acceleration structure on a command list,
// using application-provided buffers and possibly a pointer to the previous
// acceleration structure in case of iterative updates. Note that the update can
// be done in place: the result and previousResult pointers can be the same.
void TopLevelASGenerator::Generate(
    ID3D12GraphicsCommandList4* commandList, // Command list on which the build will be enqueued
    ID3D12Resource* scratchBuffer,           // Scratch buffer used by the builder to
    // store temporary data
    ID3D12Resource* resultBuffer,      // Result buffer storing the acceleration structure
    ID3D12Resource* descriptorsBuffer, // Auxiliary result buffer containing the instance
    // descriptors, has to be in upload heap
    bool updateOnly /*= false*/, // If true, simply refit the existing
    // acceleration structure
    ID3D12Resource* previousResult /*= nullptr*/ // Optional previous acceleration
    // structure, used if an iterative update
    // is requested
)
{
    // Copy the descriptors in the target descriptor buffer
    D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs;
    descriptorsBuffer->Map(0, nullptr, reinterpret_cast<void**>(&instanceDescs));
    if (!instanceDescs)
    {
        throw std::logic_error(
            "Cannot map the instance descriptor buffer - is it "
            "in the upload heap?");
    }

    auto instanceCount = static_cast<UINT>(m_instances.size());

    // Initialize the memory to zero on the first time only
    if (!updateOnly)
    {
        ZeroMemory(instanceDescs, m_instanceDescsSizeInBytes);
    }

    // Create the description for each instance
    for (uint32_t i = 0; i < instanceCount; i++)
    {
        // Instance ID visible in the shader in InstanceID()
        instanceDescs[i].InstanceID = m_instances[i].instanceID;
        // Index of the hit group invoked upon intersection
        instanceDescs[i].InstanceContributionToHitGroupIndex = m_instances[i].hitGroupIndex;
        // Instance flags, including backface culling, winding, etc - TODO: should
        // be accessible from outside
        instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        // Instance transform matrix
        DirectX::XMMATRIX m = XMMatrixTranspose(m_instances[i].transform); // GLM is column major, the INSTANCE_DESC is row major
        memcpy(instanceDescs[i].Transform, &m, sizeof(instanceDescs[i].Transform));
        // Get access to the bottom level
        instanceDescs[i].AccelerationStructure = m_instances[i].bottomLevelAS->GetGPUVirtualAddress();
        // Visibility mask, always visible here - TODO: should be accessible from outside
        instanceDescs[i].InstanceMask = 0xFF;
    }

    descriptorsBuffer->Unmap(0, nullptr);

    // If this in an update operation we need to provide the source buffer
    D3D12_GPU_VIRTUAL_ADDRESS pSourceAS = updateOnly ? previousResult->GetGPUVirtualAddress() : 0;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = m_flags;
    // The stored flags represent whether the AS has been built for updates or
    // not. If yes and an update is requested, the builder is told to only update
    // the AS instead of fully rebuilding it
    if (flags == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && updateOnly)
    {
        flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    }

    // Sanity checks
    if (m_flags != D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && updateOnly)
    {
        throw std::logic_error("Cannot update a top-level AS not originally built for updates");
    }
    if (updateOnly && previousResult == nullptr)
    {
        throw std::logic_error("Top-level hierarchy update requires the previous hierarchy");
    }

    // Create a descriptor of the requested builder work, to generate a top-level
    // AS from the input parameters
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    buildDesc.Inputs.InstanceDescs = descriptorsBuffer->GetGPUVirtualAddress();
    buildDesc.Inputs.NumDescs = instanceCount;
    buildDesc.DestAccelerationStructureData = {resultBuffer->GetGPUVirtualAddress()};
    buildDesc.ScratchAccelerationStructureData = {scratchBuffer->GetGPUVirtualAddress()};
    buildDesc.SourceAccelerationStructureData = pSourceAS;
    buildDesc.Inputs.Flags = flags;

    // Build the top-level AS
    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // Wait for the builder to complete by setting a barrier on the resulting
    // buffer. This can be important in case the rendering is triggered
    // immediately afterwards, without executing the command list
    D3D12_RESOURCE_BARRIER uavBarrier;
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = resultBuffer;
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    commandList->ResourceBarrier(1, &uavBarrier);
}

TopLevelASGenerator::Instance::Instance(ID3D12Resource* blAS, const DirectX::XMMATRIX& tr, UINT iID, UINT hgId) :
    bottomLevelAS(blAS), transform(tr), instanceID(iID), hitGroupIndex(hgId)
{
}
} // namespace nv_helpers_dx12
