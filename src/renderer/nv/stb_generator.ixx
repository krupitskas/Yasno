module;

#include <d3d12.h>

export module renderer.nv.sbt_generator;

import std;
import system.string_helpers;

export namespace nv_helpers_dx12
{
/// Helper class to create and maintain a Shader Binding Table
class ShaderBindingTableGenerator
{
public:
    /// Add a ray generation program by name, with its list of data pointers or values according to
    /// the layout of its root signature
    void AddRayGenerationProgram(const std::wstring& entryPoint, const std::vector<void*>& inputData);

    /// Add a miss program by name, with its list of data pointers or values according to
    /// the layout of its root signature
    void AddMissProgram(const std::wstring& entryPoint, const std::vector<void*>& inputData);

    /// Add a hit group by name, with its list of data pointers or values according to
    /// the layout of its root signature
    void AddHitGroup(const std::wstring& entryPoint, const std::vector<void*>& inputData);

    /// Compute the size of the SBT based on the set of programs and hit groups it contains
    std::uint32_t ComputeSBTSize();

    /// Build the SBT and store it into sbtBuffer, which has to be pre-allocated on the upload heap.
    /// Access to the raytracing pipeline object is required to fetch program identifiers using their
    /// names
    void Generate(ID3D12Resource* sbtBuffer, ID3D12StateObjectProperties* raytracingPipeline);

    /// Reset the sets of programs and hit groups
    void Reset();

    /// The following getters are used to simplify the call to DispatchRays where the offsets of the
    /// shader programs must be exactly following the SBT layout

    /// Get the size in bytes of the SBT section dedicated to ray generation programs
    UINT GetRayGenSectionSize() const;
    /// Get the size in bytes of one ray generation program entry in the SBT
    UINT GetRayGenEntrySize() const;

    /// Get the size in bytes of the SBT section dedicated to miss programs
    UINT GetMissSectionSize() const;
    /// Get the size in bytes of one miss program entry in the SBT
    UINT GetMissEntrySize();

    /// Get the size in bytes of the SBT section dedicated to hit groups
    UINT GetHitGroupSectionSize() const;
    /// Get the size in bytes of hit group entry in the SBT
    UINT GetHitGroupEntrySize() const;

private:
    /// Wrapper for SBT entries, each consisting of the name of the program and a list of values,
    /// which can be either pointers or raw 32-bit constants
    struct SBTEntry
    {
        SBTEntry(std::wstring entryPoint, std::vector<void*> inputData);

        const std::wstring m_entryPoint;
        const std::vector<void*> m_inputData;
    };

    /// For each entry, copy the shader identifier followed by its resource pointers and/or root
    /// constants in outputData, with a stride in bytes of entrySize, and returns the size in bytes
    /// actually written to outputData.
    std::uint32_t CopyShaderData(
        ID3D12StateObjectProperties* raytracingPipeline, uint8_t* outputData, const std::vector<SBTEntry>& shaders, std::uint32_t entrySize);

    /// Compute the size of the SBT entries for a set of entries, which is determined by the maximum
    /// number of parameters of their root signature
    std::uint32_t GetEntrySize(const std::vector<SBTEntry>& entries);

    std::vector<SBTEntry> m_rayGen;
    std::vector<SBTEntry> m_miss;
    std::vector<SBTEntry> m_hitGroup;

    /// For each category, the size of an entry in the SBT depends on the maximum number of resources
    /// used by the shaders in that category.The helper computes those values automatically in
    /// GetEntrySize()
    std::uint32_t m_rayGenEntrySize;
    std::uint32_t m_missEntrySize;
    std::uint32_t m_hitGroupEntrySize;

    /// The program names are translated into program identifiers.The size in bytes of an identifier
    /// is provided by the device and is the same for all categories.
    UINT m_progIdSize;
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
// Add a ray generation program by name, with its list of data pointers or values according to
// the layout of its root signature
void ShaderBindingTableGenerator::AddRayGenerationProgram(const std::wstring& entryPoint, const std::vector<void*>& inputData)
{
    m_rayGen.emplace_back(SBTEntry(entryPoint, inputData));
}

//--------------------------------------------------------------------------------------------------
//
// Add a miss program by name, with its list of data pointers or values according to
// the layout of its root signature
void ShaderBindingTableGenerator::AddMissProgram(const std::wstring& entryPoint, const std::vector<void*>& inputData)
{
    m_miss.emplace_back(SBTEntry(entryPoint, inputData));
}

//--------------------------------------------------------------------------------------------------
//
// Add a hit group by name, with its list of data pointers or values according to
// the layout of its root signature
void ShaderBindingTableGenerator::AddHitGroup(const std::wstring& entryPoint, const std::vector<void*>& inputData)
{
    m_hitGroup.emplace_back(SBTEntry(entryPoint, inputData));
}

//--------------------------------------------------------------------------------------------------
//
// Compute the size of the SBT based on the set of programs and hit groups it contains
std::uint32_t ShaderBindingTableGenerator::ComputeSBTSize()
{
    // Size of a program identifier
    m_progIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES; // todo(rtxbuf): was D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
    // Compute the entry size of each program type depending on the maximum number of parameters in
    // each category
    m_rayGenEntrySize = GetEntrySize(m_rayGen);
    m_missEntrySize = GetEntrySize(m_miss);
    m_hitGroupEntrySize = GetEntrySize(m_hitGroup);

    // The total SBT size is the sum of the entries for ray generation, miss and hit groups, aligned
    // on 256 bytes
    std::uint32_t sbtSize = ROUND_UP(
        m_rayGenEntrySize * static_cast<UINT>(m_rayGen.size()) + m_missEntrySize * static_cast<UINT>(m_miss.size()) +
            m_hitGroupEntrySize * static_cast<UINT>(m_hitGroup.size()),
        256);
    return sbtSize;
}

//--------------------------------------------------------------------------------------------------
//
// Build the SBT and store it into sbtBuffer, which has to be pre-allocated on the upload heap.
// Access to the raytracing pipeline object is required to fetch program identifiers using their
// names
void ShaderBindingTableGenerator::Generate(ID3D12Resource* sbtBuffer, ID3D12StateObjectProperties* raytracingPipeline)
{
    // Map the SBT
    uint8_t* pData;
    HRESULT hr = sbtBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData));
    if (FAILED(hr))
    {
        throw std::logic_error("Could not map the shader binding table");
    }
    // Copy the shader identifiers followed by their resource pointers or root constants: first the
    // ray generation, then the miss shaders, and finally the set of hit groups
    std::uint32_t offset = 0;

    offset = CopyShaderData(raytracingPipeline, pData, m_rayGen, m_rayGenEntrySize);
    pData += offset;

    offset = CopyShaderData(raytracingPipeline, pData, m_miss, m_missEntrySize);
    pData += offset;

    offset = CopyShaderData(raytracingPipeline, pData, m_hitGroup, m_hitGroupEntrySize);

    // Unmap the SBT
    sbtBuffer->Unmap(0, nullptr);
}

//--------------------------------------------------------------------------------------------------
//
// Reset the sets of programs and hit groups
void ShaderBindingTableGenerator::Reset()
{
    m_rayGen.clear();
    m_miss.clear();
    m_hitGroup.clear();

    m_rayGenEntrySize = 0;
    m_missEntrySize = 0;
    m_hitGroupEntrySize = 0;
    m_progIdSize = 0;
}

//--------------------------------------------------------------------------------------------------
// The following getters are used to simplify the call to DispatchRays where the offsets of the
// shader programs must be exactly following the SBT layout

//--------------------------------------------------------------------------------------------------
//
// Get the size in bytes of the SBT section dedicated to ray generation programs
UINT ShaderBindingTableGenerator::GetRayGenSectionSize() const
{
    return m_rayGenEntrySize * static_cast<UINT>(m_rayGen.size());
}

//--------------------------------------------------------------------------------------------------
//
// Get the size in bytes of one ray generation program entry in the SBT
UINT ShaderBindingTableGenerator::GetRayGenEntrySize() const
{
    return m_rayGenEntrySize;
}

//--------------------------------------------------------------------------------------------------
//
// Get the size in bytes of the SBT section dedicated to miss programs
UINT ShaderBindingTableGenerator::GetMissSectionSize() const
{
    return m_missEntrySize * static_cast<UINT>(m_miss.size());
}

//--------------------------------------------------------------------------------------------------
//
// Get the size in bytes of one miss program entry in the SBT
UINT ShaderBindingTableGenerator::GetMissEntrySize()
{
    return m_missEntrySize;
}

//--------------------------------------------------------------------------------------------------
//
// Get the size in bytes of the SBT section dedicated to hit groups
UINT ShaderBindingTableGenerator::GetHitGroupSectionSize() const
{
    return m_hitGroupEntrySize * static_cast<UINT>(m_hitGroup.size());
}

//--------------------------------------------------------------------------------------------------
//
// Get the size in bytes of one hit group entry in the SBT
UINT ShaderBindingTableGenerator::GetHitGroupEntrySize() const
{
    return m_hitGroupEntrySize;
}

//--------------------------------------------------------------------------------------------------
//
// For each entry, copy the shader identifier followed by its resource pointers and/or root
// constants in outputData, with a stride in bytes of entrySize, and returns the size in bytes
// actually written to outputData.
std::uint32_t ShaderBindingTableGenerator::CopyShaderData(
    ID3D12StateObjectProperties* raytracingPipeline, uint8_t* outputData, const std::vector<SBTEntry>& shaders, std::uint32_t entrySize)
{
    uint8_t* pData = outputData;
    for (const auto& shader : shaders)
    {
        // Get the shader identifier, and check whether that identifier is known
        void* id = raytracingPipeline->GetShaderIdentifier(shader.m_entryPoint.c_str());
        if (!id)
        {
            std::wstring errMsg(std::wstring(L"Unknown shader identifier used in the SBT: ") + shader.m_entryPoint);

            throw std::logic_error(ysn::WStringToString(errMsg));
        }
        // Copy the shader identifier
        memcpy(pData, id, m_progIdSize);
        // Copy all its resources pointers or values in bulk
        memcpy(pData + m_progIdSize, shader.m_inputData.data(), shader.m_inputData.size() * 8);

        pData += entrySize;
    }
    // Return the number of bytes actually written to the output buffer
    return static_cast<std::uint32_t>(shaders.size()) * entrySize;
}

//--------------------------------------------------------------------------------------------------
//
// Compute the size of the SBT entries for a set of entries, which is determined by the maximum
// number of parameters of their root signature
std::uint32_t ShaderBindingTableGenerator::GetEntrySize(const std::vector<SBTEntry>& entries)
{
    // Find the maximum number of parameters used by a single entry
    size_t maxArgs = 0;
    for (const auto& shader : entries)
    {
        maxArgs = std::max(maxArgs, shader.m_inputData.size());
    }
    // A SBT entry is made of a program ID and a set of parameters, taking 8 bytes each. Those
    // parameters can either be 8-bytes pointers, or 4-bytes constants
    std::uint32_t entrySize = m_progIdSize + 8 * static_cast<std::uint32_t>(maxArgs);

    // The entries of the shader binding table must be 16-bytes-aligned
    entrySize = ROUND_UP(entrySize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT); // todo(rtxbuf): was D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT

    return entrySize;
}

//--------------------------------------------------------------------------------------------------
//
//
ShaderBindingTableGenerator::SBTEntry::SBTEntry(std::wstring entryPoint, std::vector<void*> inputData) :
    m_entryPoint(std::move(entryPoint)), m_inputData(std::move(inputData))
{
}
} // namespace nv_helpers_dx12
