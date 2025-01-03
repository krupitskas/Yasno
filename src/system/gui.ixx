module;

#include <wil/com.h>
#include <d3d12.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

export module system.gui;

import std;
import renderer.dxrenderer;
import renderer.dx_types;
import system.application;

export namespace ysn
{
const std::string CONTROLS_NAME = "Controls";
const std::string STATS_NAME = "Stats";

void InitializeImgui(std::shared_ptr<ysn::Window> window, std::shared_ptr<DxRenderer> renderer);
void ShutdownImgui();
void ImguiPrepareNewFrame();
void ImguiRenderFrame(wil::com_ptr<DxGraphicsCommandList> command_list);
} // namespace ysn

module :private;

namespace ysn
{
static bool is_docking_initialized = false;

void SetupColorScheme()
{
    ImGui::StyleColorsDark();
}

void InitializeImgui(std::shared_ptr<ysn::Window> window, std::shared_ptr<DxRenderer> renderer)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    ysn::SetupColorScheme();

    ImGui_ImplWin32_Init(window->GetWindowHandle());

    auto imgui_srv_descriptor = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

    ImGui_ImplDX12_Init(
        renderer->GetDevice().get(),
        window->BufferCount,
        renderer->GetBackBufferFormat(),
        renderer->GetCbvSrvUavDescriptorHeap()->GetHeapPtr(),
        imgui_srv_descriptor.cpu,
        imgui_srv_descriptor.gpu);
}

void ShutdownImgui()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImguiPrepareNewFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    const auto dockspace_id = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    if (!is_docking_initialized)
    {
        ImGuiID dock_id_right = 0;
        ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.15f, nullptr, &dock_id_right);
        dock_id_right = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Right, 0.15f, nullptr, nullptr);

        ImGui::DockBuilderDockWindow(CONTROLS_NAME.c_str(), dock_id_left);
        ImGui::DockBuilderDockWindow(STATS_NAME.c_str(), dock_id_right);
        ImGui::DockBuilderFinish(dockspace_id);


        is_docking_initialized = true;
    }
}

void ImguiRenderFrame(wil::com_ptr<DxGraphicsCommandList> command_list)
{
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), command_list.get());
}
} // namespace ysn