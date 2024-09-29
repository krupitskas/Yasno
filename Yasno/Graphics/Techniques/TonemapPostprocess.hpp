#pragma once

#include <memory>

#include <wil/com.h>
#include <d3d12.h>

#include <Renderer/DescriptorHeap.hpp>

namespace ysn
{
	class CommandQueue;
	class CbvSrvUavDescriptorHeap;

	enum class TonemapMethod : uint8_t
	{
		None,
		Reinhard,
		Aces
	};

	YSN_SHADER_STRUCT TonemapParameters
	{
		uint32_t display_width	= 0;
		uint32_t display_height = 0;
		uint32_t tonemap_method = 0;
		float exposure			= 0.0f;
	};

	struct TonemapPostprocessParameters
	{
		std::shared_ptr<CommandQueue> command_queue = nullptr;
		std::shared_ptr<CbvSrvUavDescriptorHeap> cbv_srv_uav_heap = nullptr;
		wil::com_ptr<ID3D12Resource> scene_color_buffer = nullptr;
		DescriptorHandle hdr_uav_descriptor_handle;
		DescriptorHandle backbuffer_uav_descriptor_handle;
		uint32_t backbuffer_width = 0;
		uint32_t backbuffer_height = 0;
	};

	struct TonemapPostprocess
	{
		bool Initialize();
		void Render(TonemapPostprocessParameters* parameters);
		
		wil::com_ptr<ID3D12Resource> parameters_buffer;

		wil::com_ptr<ID3D12RootSignature> root_signature;
		wil::com_ptr<ID3D12PipelineState> pipeline_state;

		float exposure = 1.0f;
		TonemapMethod tonemap_method = TonemapMethod::None;
	};
}
