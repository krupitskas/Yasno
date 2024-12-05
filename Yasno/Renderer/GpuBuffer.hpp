#pragma once

import <optional>;

import renderer.gpu_resource;

#include <System/Application.hpp>
#include <Renderer/DxRenderer.hpp>

namespace ysn
{
	struct GpuBuffer : public GpuResource
	{};

	struct GpuBufferCreateInfo
	{
		UINT64 size = 0;
		UINT64 alignment = 0;
		D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
		D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
	};

	std::optional<GpuBuffer> CreateGpuBuffer(const GpuBufferCreateInfo& info, const std::string& debug_name);

	bool UploadToGpuBuffer(wil::com_ptr<ID3D12GraphicsCommandList4> cmd_list, const GpuBuffer& target_buffer, const void* upload_data, const std::vector<D3D12_SUBRESOURCE_DATA>& subresource_data, D3D12_RESOURCE_STATES target_state);
}