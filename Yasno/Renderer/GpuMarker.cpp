#include "GpuMarker.hpp"

#include <imgui.h>

#include "pix3.h"

namespace ysn
{
	GpuMarker::GpuMarker(wil::com_ptr<ID3D12GraphicsCommandList4> command_list, std::string_view name)
	{
	#ifndef YSN_RELEASE
		cmd_list = command_list;
		PIXBeginEvent(cmd_list.get(), PIX_COLOR_DEFAULT, name.data());
	#else
		YSN_UNUSED(command_list);
		YSN_UNUSED(name);
	#endif
	}

	void GpuMarker::EndEvent()
	{
	#ifndef YSN_RELEASE
		PIXEndEvent(cmd_list.get());
	#endif
	}
}
