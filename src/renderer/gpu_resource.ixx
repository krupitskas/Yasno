module;

#include <d3dx12.h>
#include <wil/com.h>

export module renderer.gpu_resource;

import system.compilation;

export namespace ysn
{
class GpuResource
{
public:
    GpuResource() = default;
    GpuResource(ID3D12Resource* res);
    ~GpuResource();

    virtual void Destroy();

    ID3D12Resource** AddressOf();
    D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddress() const;
    wil::com_ptr<ID3D12Resource> ComPtr() const;
    D3D12_RESOURCE_DESC Desc() const;

    void SetResource(ID3D12Resource* res);
    void SetName(const std::wstring& name) const;
    void SetName(const std::string& name) const;

    ID3D12Resource* operator->();
    const ID3D12Resource* operator->() const;
    ID3D12Resource* Resource();
    const ID3D12Resource* Resource() const;

protected:
    wil::com_ptr<ID3D12Resource> m_resource;
};

} // namespace ysn

module :private;

namespace ysn
{
GpuResource::GpuResource(ID3D12Resource* res)
{
    SetResource(res);
}

GpuResource ::~GpuResource()
{
    Destroy();
}

void GpuResource::Destroy()
{
    m_resource = nullptr;
}

ID3D12Resource** GpuResource::AddressOf()
{
    return m_resource.addressof();
}

D3D12_GPU_VIRTUAL_ADDRESS GpuResource::GPUVirtualAddress() const
{
    return m_resource->GetGPUVirtualAddress();
}

wil::com_ptr<ID3D12Resource> GpuResource::ComPtr() const
{
    return m_resource;
}

D3D12_RESOURCE_DESC GpuResource::Desc() const
{
    return m_resource->GetDesc();
}

void GpuResource::SetResource(ID3D12Resource* res)
{
    m_resource.attach(res);
}

void GpuResource::SetName(const std::string& name) const
{
    std::wstring wname(name.begin(), name.end());
    SetName(wname);
}

void GpuResource::SetName(const std::wstring& name) const
{
    if constexpr (!IsReleaseActive())
    {
        m_resource->SetName(name.c_str());
    }
}

ID3D12Resource* GpuResource::operator->()
{
    return m_resource.get();
}

const ID3D12Resource* GpuResource::operator->() const
{
    return m_resource.get();
}

ID3D12Resource* GpuResource::Resource()
{
    return m_resource.get();
}

const ID3D12Resource* GpuResource::Resource() const
{
    return m_resource.get();
}

} // namespace ysn