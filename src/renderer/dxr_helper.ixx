module;

#include <d3d12.h>

export module renderer.dxr_helper;

import std;
import system.helpers;

export namespace nv_helpers_dx12
{

#ifndef ROUND_UP
#define ROUND_UP(v, powerOf2Alignment) (((v) + (powerOf2Alignment) - 1) & ~((powerOf2Alignment) - 1))
#endif

// Specifies a heap used for uploading. This heap type has CPU access optimized
// for uploading to the GPU.
const D3D12_HEAP_PROPERTIES kUploadHeapProps = {D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0};

// Specifies the default heap. This heap type experiences the most bandwidth for
// the GPU, but cannot provide CPU access.
const D3D12_HEAP_PROPERTIES kDefaultHeapProps = {
    D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0};

// ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device* device, uint32_t count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible)
//{
//     D3D12_DESCRIPTOR_HEAP_DESC desc = {};
//     desc.NumDescriptors = count;
//     desc.Type = type;
//     desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

//    ID3D12DescriptorHeap* pHeap;
//    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap)));
//    return pHeap;
//}
} // namespace nv_helpers_dx12
