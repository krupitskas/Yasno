module;

#include <d3dx12.h>
#include <wil/com.h>

export module renderer.gpu_buffer;

import std;
import renderer.gpu_resource;
import renderer.dxrenderer;
import system.application;
import system.logger;

export namespace ysn
{
struct GpuBuffer : public GpuResource
{
};

struct GpuBufferCreateInfo
{
    UINT64 size = 0;
    UINT64 alignment = 0;
    D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
};

std::optional<GpuBuffer> CreateGpuBuffer(const GpuBufferCreateInfo& info, const std::string& debug_name);

bool UploadToGpuBuffer(
    wil::com_ptr<ID3D12GraphicsCommandList4> cmd_list,
    const GpuBuffer& target_buffer,
    const void* upload_data,
    const std::vector<D3D12_SUBRESOURCE_DATA>& subresource_data,
    D3D12_RESOURCE_STATES target_state);
} // namespace ysn

module :private;

namespace ysn
{
std::optional<GpuBuffer> CreateGpuBuffer(const GpuBufferCreateInfo& info, const std::string& debug_name)
{
    auto d3d_device = Application::Get().GetDevice();

    D3D12_HEAP_PROPERTIES heap_desc = {};
    heap_desc.Type = info.heap_type;
    heap_desc.CreationNodeMask = 1;
    heap_desc.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc = {};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Alignment = info.alignment;
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Width = info.size;
    resource_desc.Flags = info.flags;

    GpuBuffer result_buffer;

    const HRESULT hr = d3d_device->CreateCommittedResource(
        &heap_desc, D3D12_HEAP_FLAG_NONE, &resource_desc, info.state, nullptr, IID_PPV_ARGS(&result_buffer.resource));

    if (hr != S_OK)
    {
        LogError << "Can't create GPU buffer " << debug_name;
        return std::nullopt;
    }
    else
    {

#ifndef YSN_RELEASE
        std::wstring name(debug_name.begin(), debug_name.end());
        result_buffer.resource->SetName(name.c_str());
#endif

        return result_buffer;
    }
}

bool UploadToGpuBuffer(
    wil::com_ptr<ID3D12GraphicsCommandList4> cmd_list,
    const GpuBuffer& target_buffer,
    const void* upload_data,
    const std::vector<D3D12_SUBRESOURCE_DATA>& subresource_data,
    D3D12_RESOURCE_STATES target_state)
{
    const bool subresources_specified = !subresource_data.empty();
    const UINT subresource_count = subresources_specified ? UINT(subresource_data.size()) : 1;
    const UINT64 required_size = GetRequiredIntermediateSize(target_buffer.resource.get(), 0, subresource_count);

    // Align buffer for texture uploads (this may not be necessary for other types of buffers)
    const UINT64 upload_buffer_size = D3DX12Align<UINT64>(required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

    // Allocate temporary buffer on the upload heap
    GpuBufferCreateInfo info_upload{
        .size = upload_buffer_size, .heap_type = D3D12_HEAP_TYPE_UPLOAD, .state = D3D12_RESOURCE_STATE_GENERIC_READ};

    const auto gpu_buffer_result = CreateGpuBuffer(info_upload, "Upload Buffer");

    if (!gpu_buffer_result.has_value())
    {
        LogError << "Can't create gpu upload buffer\n";
        return false;
    }

    const GpuBuffer& upload_buffer = gpu_buffer_result.value();

    // Keep in stage
    Application::Get().GetRenderer()->stage_heap.push_back(upload_buffer.resource);

    // Build description of subresource data or use the one provided
    const D3D12_SUBRESOURCE_DATA* buffer_data_desc_ptr = nullptr;

    if (!subresources_specified)
    {
        D3D12_SUBRESOURCE_DATA buffer_data_desc = {};
        buffer_data_desc.pData = upload_data;
        buffer_data_desc.RowPitch = upload_buffer_size;
        buffer_data_desc.SlicePitch = upload_buffer_size;

        buffer_data_desc_ptr = &buffer_data_desc;
    }
    else
    {
        buffer_data_desc_ptr = subresource_data.data();
    }

    // Schedule a copy from the upload heap to the device memory
    UINT64 uploaded_bytes = UpdateSubresources(
        cmd_list.get(), target_buffer.resource.get(), upload_buffer.resource.get(), 0, 0, subresource_count, buffer_data_desc_ptr);
    HRESULT hr = (required_size == uploaded_bytes ? S_OK : E_FAIL);

    if (hr != S_OK)
    {
        LogError << "Failed to upload data via upload heap\n";
        return false;
    }

    // Transition resource to target state
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = target_buffer.resource.get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = target_state;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd_list->ResourceBarrier(1, &barrier);

    return true;
}
} // namespace ysn
