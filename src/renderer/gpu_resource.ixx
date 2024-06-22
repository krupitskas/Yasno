module;

#include <d3dx12.h>
#include <wil/com.h>

export module renderer.gpu_resource;

export namespace ysn
{
struct GpuResource
{
    GpuResource() = default;
    GpuResource(wil::com_ptr<ID3D12Resource> res) : resource(res)
    {
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const
    {
        return resource->GetGPUVirtualAddress();
    }

    wil::com_ptr<ID3D12Resource> resource;
};
} // namespace ysn