module;

#include <d3d12.h>
#include <wil/com.h>
#include <d3dx12.h>
#include <stb_image.h>

export module renderer.gpu_texture;

import std;
import system.application;
import system.logger;
import renderer.dxrenderer;
import renderer.descriptor_heap;
import system.helpers;

export namespace ysn
{
struct GpuTexture
{
    std::wstring name = L"Unnamed Texture";

    bool is_srgb = false;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t num_mips = 0;

    wil::com_ptr<ID3D12Resource> gpu_resource;

    DescriptorHandle descriptor_handle;
};

struct LoadTextureParameters
{
    std::string filename = "";
    wil::com_ptr<ID3D12GraphicsCommandList4> command_list;
    bool generate_mips = false;
    bool is_srgb = false;
};

std::optional<GpuTexture> LoadTextureFromFile(const LoadTextureParameters& parameters);
} // namespace ysn

module :private;

namespace ysn
{

static inline uint32_t ComputeNumMips(uint32_t Width, uint32_t Height)
{
    uint32_t HighBit;
    _BitScanReverse((unsigned long*)&HighBit, Width | Height);
    return HighBit + 1;
}

std::optional<GpuTexture> LoadTextureFromFile(const LoadTextureParameters& parameters)
{
    std::shared_ptr<DxRenderer> renderer = Application::Get().GetRenderer();

    GpuTexture result_texture;

    // TODO(task): Make it dynamic
    // The number of bytes used to represent a pixel in the texture.
    int32_t texture_pixel_size = 0;

    int32_t width, height, tex_channels = 0;

    const bool is_hdr_image = stbi_is_hdr(parameters.filename.c_str());

    unsigned char* ldr_data = nullptr;
    float* hdr_data = nullptr;

    if (is_hdr_image)
    {
        texture_pixel_size = 16;
        hdr_data = stbi_loadf(parameters.filename.c_str(), &width, &height, &tex_channels, STBI_rgb_alpha);
    }
    else
    {
        texture_pixel_size = 4;
        ldr_data = stbi_load(parameters.filename.c_str(), &width, &height, &tex_channels, STBI_rgb_alpha);
    }

    if (ldr_data != nullptr || hdr_data != nullptr)
    {
        uint32_t num_mips = 1;

        if (parameters.generate_mips)
        {
            num_mips = ComputeNumMips(width, height);
        }

        DXGI_FORMAT format;

        if (is_hdr_image)
        {
            if (parameters.is_srgb)
            {
                LogInfo << "Trying to use HDR texture " << parameters.filename << " as SRGB\n";
            }

            format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        }
        else
        {
            if (parameters.is_srgb)
            {
                format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            }
            else
            {
                format = DXGI_FORMAT_R8G8B8A8_UNORM;
            }
        }

        D3D12_RESOURCE_DESC texture_desc = {};
        texture_desc.Format = format;
        texture_desc.MipLevels = static_cast<UINT16>(num_mips);
        texture_desc.Width = width;
        texture_desc.Height = height;
        texture_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        texture_desc.DepthOrArraySize = 1;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.SampleDesc.Quality = 0;
        texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        const CD3DX12_HEAP_PROPERTIES heap_properties_default = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        const CD3DX12_HEAP_PROPERTIES heap_properties_upload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

        HRESULT result = renderer->GetDevice()->CreateCommittedResource(
            &heap_properties_default,
            D3D12_HEAP_FLAG_NONE,
            &texture_desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&result_texture.gpu_resource));

        if (result != S_OK)
        {
            LogError << "Can't create GPU texture " << parameters.filename << "\n";
            return std::nullopt;
        }

#ifndef YSN_RELEASE
        std::wstring name(parameters.filename.begin(), parameters.filename.end());
        result_texture.gpu_resource->SetName(name.c_str());
#endif

        const UINT64 upload_buffer_size = GetRequiredIntermediateSize(result_texture.gpu_resource.get(), 0, 1);

        const auto upload_buffer_size_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);

        wil::com_ptr<ID3D12Resource> texture_upload_heap;

        // Create the GPU upload buffer.
        result = renderer->GetDevice()->CreateCommittedResource(
            &heap_properties_upload,
            D3D12_HEAP_FLAG_NONE,
            &upload_buffer_size_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&texture_upload_heap));

        if (result != S_OK)
        {
            LogError << "Can't create GPU upload heap texture " << parameters.filename << "\n";
            return std::nullopt;
        }

        renderer->stage_heap.push_back(texture_upload_heap);

        D3D12_SUBRESOURCE_DATA texture_data = {};
        texture_data.pData = is_hdr_image ? (void*)hdr_data : (void*)ldr_data;
        texture_data.RowPitch = width * texture_pixel_size;
        texture_data.SlicePitch = texture_data.RowPitch * height;

        UpdateSubresources(
            parameters.command_list.get(), result_texture.gpu_resource.get(), texture_upload_heap.get(), 0, 0, 1, &texture_data);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = texture_desc.Format;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = num_mips;

        const DescriptorHandle srv_handle = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

        renderer->GetDevice()->CreateShaderResourceView(result_texture.gpu_resource.get(), &srv_desc, srv_handle.cpu);

        result_texture.descriptor_handle = srv_handle;
        result_texture.num_mips = num_mips;
        result_texture.is_srgb = parameters.is_srgb;

        if (is_hdr_image)
        {
            stbi_image_free(hdr_data);
        }
        else
        {
            stbi_image_free(ldr_data);
        }

        if (parameters.generate_mips)
        {
            // TODO(modules): restore
            // renderer->GetMipGenerator()->GenerateMips(renderer, parameters.command_list, result_texture);
        }

        // const CD3DX12_RESOURCE_BARRIER barrier_desc = CD3DX12_RESOURCE_BARRIER::Transition(result_texture.gpu_resource.get(),
        // D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); parameters.command_list->ResourceBarrier(1,
        // &barrier_desc);
    }
    else
    {
        LogError << "Can't load " << parameters.filename << " texture\n";
        return std::nullopt;
    }

    return result_texture;
}

/*

Texture LoadHDRTextureFromFile(const std::string& filename,
wil::com_ptr<ID3D12GraphicsCommandList2> CommandList,
wil::com_ptr<ID3D12Device5> device,
const ysn::DescriptorHandle& srv_handle)
{
Texture result;

// TODO(task): Make it dynamic
// The number of bytes used to represent a pixel in the texture.
int texturePixelSize = 4 * 4;

int width, height, tex_channels;
float* data = stbi_loadf(filename.c_str(), &width, &height, &tex_channels, STBI_rgb_alpha);

if (data != nullptr)
{
D3D12_RESOURCE_DESC texture_desc = {};
texture_desc.MipLevels = 1;
texture_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // TODO: Sheesh its too fat!
texture_desc.Width = width;
texture_desc.Height = height;
texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
texture_desc.DepthOrArraySize = 1;
texture_desc.SampleDesc.Count = 1;
texture_desc.SampleDesc.Quality = 0;
texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

const auto heap_properties_default = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
const auto heap_properties_upload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

ThrowIfFailed(device->CreateCommittedResource(
&heap_properties_default,
D3D12_HEAP_FLAG_NONE,
&texture_desc,
D3D12_RESOURCE_STATE_COPY_DEST,
nullptr,
IID_PPV_ARGS(&result.gpuTexture)));

#ifndef YSN_RELEASE
std::wstring name(filename.begin(), filename.end());
result.gpuTexture->SetName(name.c_str());
#endif

const UINT64 upload_buffer_size = GetRequiredIntermediateSize(result.gpuTexture.get(), 0, 1);

const auto upload_buffer_size_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);

// Create the GPU upload buffer.
ThrowIfFailed(device->CreateCommittedResource(
&heap_properties_upload,
D3D12_HEAP_FLAG_NONE,
&upload_buffer_size_desc,
D3D12_RESOURCE_STATE_GENERIC_READ,
nullptr,
IID_PPV_ARGS(&result.textureUploadHeap)));

D3D12_SUBRESOURCE_DATA textureData = {};
textureData.pData = data;
textureData.RowPitch = width * texturePixelSize;
textureData.SlicePitch = textureData.RowPitch * height;

const CD3DX12_RESOURCE_BARRIER barrier_desc = CD3DX12_RESOURCE_BARRIER::Transition(
result.gpuTexture.get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

UpdateSubresources(CommandList.get(), result.gpuTexture.get(), result.textureUploadHeap.get(), 0, 0, 1, &textureData);

// CommandList->ResourceBarrier(1, &barrierDesc);

// Describe and create a SRV for the texture.
D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
srvDesc.Format = texture_desc.Format;
srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
srvDesc.Texture2D.MipLevels = 1;

device->CreateShaderResourceView(result.gpuTexture.get(), &srvDesc, srv_handle.cpu);

result.descriptor_handle = srv_handle;

stbi_image_free(data);
}

return result;
}
*/
} // namespace ysn
