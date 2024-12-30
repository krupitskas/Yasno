module;

#include <DirectXMath.h>
#include <d3dx12.h>
#include <wil/com.h>

#include <shader_structs.h>

export module graphics.techniques.generate_mips_pass;

import std;
import system.filesystem;
import system.logger;
import system.asserts;
import renderer.dxrenderer;
import renderer.gpu_texture;
import renderer.dx_types;

namespace ysn
{
namespace RootSignatureIndex
{
    enum
    {
        GenerateMipsCB,
        SrcMip,
        OutMip,
        NumParameters
    };
}
} // namespace ysn

export namespace ysn
{
class GenerateMipsPass
{
public:
    bool Initialize(std::shared_ptr<DxRenderer> renderer);
    bool GenerateMips(std::shared_ptr<DxRenderer> renderer, wil::com_ptr<DxGraphicsCommandList> command_list, const GpuTexture& gpu_texture);

private:
    wil::com_ptr<ID3D12RootSignature> m_root_signature;
    wil::com_ptr<ID3D12PipelineState> m_pipeline_state;
};
} // namespace ysn

module :private;

namespace ysn
{
bool GenerateMipsPass::Initialize(std::shared_ptr<DxRenderer> renderer)
{
    CD3DX12_DESCRIPTOR_RANGE src_mip(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0);
    CD3DX12_DESCRIPTOR_RANGE out_mip(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0, 0, 0);

    CD3DX12_ROOT_PARAMETER root_parameters[RootSignatureIndex::NumParameters] = {};
    root_parameters[RootSignatureIndex::GenerateMipsCB].InitAsConstants(sizeof(GenerateMipsConstantBuffer) / sizeof(uint32_t), 0);
    root_parameters[RootSignatureIndex::SrcMip].InitAsDescriptorTable(1, &src_mip);
    root_parameters[RootSignatureIndex::OutMip].InitAsDescriptorTable(1, &out_mip);

    CD3DX12_STATIC_SAMPLER_DESC linear_clamp_sampler(
        0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
    root_signature_desc.NumParameters = RootSignatureIndex::NumParameters;
    root_signature_desc.pParameters = &root_parameters[0];
    root_signature_desc.NumStaticSamplers = 1;
    root_signature_desc.pStaticSamplers = &linear_clamp_sampler;

    if (!renderer->CreateRootSignature(&root_signature_desc, &m_root_signature))
    {
        LogError << "Generate mip pass can't create root signature\n";
        return false;
    }

    ShaderCompileParameters generate_mips_shader_parameters(ShaderType::Compute, VfsPath(L"shaders/generate_mips.cs.hlsl"));
    const auto generate_mips_shader = renderer->GetShaderStorage()->CompileShader(generate_mips_shader_parameters);

    if (!generate_mips_shader.has_value())
    {
        LogError << "Can't load generate mips shader\n";
        return false;
    }

    // Create the PSO for GenerateMips shader.
    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    } pipeline_state_stream;

    pipeline_state_stream.pRootSignature = m_root_signature.get();
    pipeline_state_stream.CS = {generate_mips_shader.value()->GetBufferPointer(), generate_mips_shader.value()->GetBufferSize()};

    D3D12_PIPELINE_STATE_STREAM_DESC pipeline_state_stream_desc = {sizeof(PipelineStateStream), &pipeline_state_stream};

    if (auto result = renderer->GetDevice()->CreatePipelineState(&pipeline_state_stream_desc, IID_PPV_ARGS(&m_pipeline_state));
        result != S_OK)
    {
        LogError << "Can't create generate mips pipeline state\n";
        return false;
    }

    // Create some default texture UAV's to pad any unused UAV's during mip map generation.
    // m_DefaultUAV = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4);

    // for (UINT i = 0; i < 4; ++i)
    //{
    //	D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
    //	uav_desc.ViewDimension			= D3D12_UAV_DIMENSION_TEXTURE2D;
    //	uav_desc.Format					= DXGI_FORMAT_R8G8B8A8_UNORM;
    //	uav_desc.Texture2D.MipSlice		= i;
    //	uav_desc.Texture2D.PlaneSlice	= 0;
    //
    //	device->CreateUnorderedAccessView(nullptr, nullptr, &uav_desc,m_DefaultUAV.GetDescriptorHandle(i));
    // }

    return true;
}

bool GenerateMipsPass::GenerateMips(std::shared_ptr<DxRenderer> renderer, wil::com_ptr<DxGraphicsCommandList> command_list, const GpuTexture& gpu_texture)
{
    auto compute_queue = renderer->GetComputeQueue();

    auto resource = gpu_texture.gpu_resource;
    auto resource_desc = resource->GetDesc();

    if (resource_desc.MipLevels == 1)
    {
        LogInfo << "Texture requested only single mip, can't generate mips\n";
        return false;
    }

    if (resource_desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || resource_desc.DepthOrArraySize != 1 ||
        resource_desc.SampleDesc.Count > 1)
    {
        LogError << "GenerateMips is only supported for non-multi-sampled 2D Textures.\n";
        return false;
    }

    ID3D12DescriptorHeap* ppHeaps[] = {renderer->GetCbvSrvUavDescriptorHeap()->GetHeapPtr()};

    command_list->SetPipelineState(m_pipeline_state.get());
    command_list->SetComputeRootSignature(m_root_signature.get());
    command_list->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    command_list->SetComputeRootDescriptorTable(RootSignatureIndex::SrcMip, gpu_texture.descriptor_handle.gpu);

    GenerateMipsConstantBuffer generate_mips_parameters;
    generate_mips_parameters.is_srgb = gpu_texture.is_srgb;

    {
        CD3DX12_RESOURCE_BARRIER barrier =
            CD3DX12_RESOURCE_BARRIER::Transition(resource.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        command_list->ResourceBarrier(1, &barrier);
    }

    for (uint32_t src_mip = 0; src_mip < resource_desc.MipLevels - 1u;)
    {
        uint64_t src_width = resource_desc.Width >> src_mip;
        uint32_t src_height = resource_desc.Height >> src_mip;
        uint32_t dst_width = static_cast<uint32_t>(src_width / 2.0f);
        uint32_t dst_height = static_cast<uint32_t>(src_height / 2.0f);

        // 0b00(0): Both width and height are even.
        // 0b01(1): Width is odd, height is even.
        // 0b10(2): Width is even, height is odd.
        // 0b11(3): Both width and height are odd.
        generate_mips_parameters.src_dimension = (src_height & 1) << 1 | (src_width & 1);

        // How many mipmap levels to compute this pass (max 4 mips per pass)
        DWORD mip_count;

        // The number of times we can half the size of the texture and get
        // exactly a 50% reduction in size.
        // A 1 bit in the width or height indicates an odd dimension.
        // The case where either the width or the height is exactly 1 is handled
        // as a special case (as the dimension does not require reduction).
        _BitScanForward(&mip_count, (dst_width == 1 ? dst_height : dst_width) | (dst_height == 1 ? dst_width : dst_height));

        // Maximum number of mips to generate is 4.
        mip_count = std::min<DWORD>(4, mip_count + 1);
        // Clamp to total number of mips left over.
        mip_count = (src_mip + mip_count) >= resource_desc.MipLevels ? resource_desc.MipLevels - src_mip - 1 : mip_count;

        // Dimensions should not reduce to 0.
        // This can happen if the width and height are not the same.
        dst_width = std::max<DWORD>(1, dst_width);
        dst_height = std::max<DWORD>(1, dst_height);

        generate_mips_parameters.src_mip_level = src_mip;
        generate_mips_parameters.num_mip_levels = mip_count;
        generate_mips_parameters.texel_size.x = 1.0f / (float)dst_width;
        generate_mips_parameters.texel_size.y = 1.0f / (float)dst_height;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_orig_desc = {};
        uav_orig_desc.Format =
            DXGI_FORMAT_R8G8B8A8_UNORM; // TODO: If srgb -> rollback to DXGI_FORMAT_R8G8B8A8_UNORM, check DXGI_FORMAT PixelBuffer::GetUAVFormat( DXGI_FORMAT defaultFormat )
        uav_orig_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uav_orig_desc.Texture2D.MipSlice = src_mip + 1;

        const auto uav_handle = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
        renderer->GetDevice()->CreateUnorderedAccessView(gpu_texture.gpu_resource.get(), nullptr, &uav_orig_desc, uav_handle.cpu);

        for (uint32_t mip = 1; mip < mip_count; mip++)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
            uav_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // TODO: If srgb -> rollback to DXGI_FORMAT_R8G8B8A8_UNORM, check DXGI_FORMAT PixelBuffer::GetUAVFormat( DXGI_FORMAT defaultFormat )
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uav_desc.Texture2D.MipSlice = src_mip + mip + 1;

            renderer->GetDevice()->CreateUnorderedAccessView(
                gpu_texture.gpu_resource.get(), nullptr, &uav_desc, renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle().cpu);
        }

        command_list->SetComputeRoot32BitConstants(
            RootSignatureIndex::GenerateMipsCB, sizeof(GenerateMipsConstantBuffer) / sizeof(uint32_t), &generate_mips_parameters, 0);
        command_list->SetComputeRootDescriptorTable(RootSignatureIndex::OutMip, uav_handle.gpu);

        command_list->Dispatch(UINT(std::ceil(dst_width / (float)8)), UINT(std::ceil(dst_height / (float)8)), 1);

        {
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.get());
            command_list->ResourceBarrier(1, &barrier);
        }

        src_mip += mip_count;
    }

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            resource.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        command_list->ResourceBarrier(1, &barrier);
    }

    return true;
}
} // namespace ysn
