#pragma once

#include <d3dx12.h>
#include <wil/com.h>

namespace ysn
{
    class GpuResource
    {
    public:

    private:
        wil::com_ptr<ID3D12Resource> m_resource;
    };
}