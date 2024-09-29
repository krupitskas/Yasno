#pragma once

#include <string_view>

#include <wil/com.h>
#include <d3dx12.h>

namespace ysn
{
	struct GpuMarker
	{
		GpuMarker(wil::com_ptr<ID3D12GraphicsCommandList4> command_list, std::string_view name);
		
		void EndEvent();

		GpuMarker& operator=(const GpuMarker& m) = delete;

	#ifndef YSN_RELEASE
		wil::com_ptr<ID3D12GraphicsCommandList4> cmd_list;
	#endif
	};
}
