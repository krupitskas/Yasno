#pragma once

#include <wil/com.h>
#include <d3d12.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

namespace ysn
{
	const static char* CONTROLS_NAME = "Controls";
	const static char* STATS_NAME = "Stats";

	class Window;
	class DxRenderer;

	void InitializeImgui(std::shared_ptr<Window> window, std::shared_ptr<DxRenderer> renderer);
	void ShutdownImgui();
	void ImguiPrepareNewFrame();
	void ImguiRenderFrame(wil::com_ptr<ID3D12GraphicsCommandList4> command_list);
}
