#pragma once

#include <d3dx12.h>
#include <DirectXMath.h>

#include <Renderer/GpuResource.hpp>

namespace ysn
{
    using IndexBufferFormat = std::uint32_t;

    struct IndexStorage
    {
        uint32_t GetIndexBufferSize();

        std::vector<IndexBufferFormat> index_data;

        GpuResource gpu_index_buffer;
    };
}