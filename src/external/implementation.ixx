module;

#pragma warning(push)
#pragma warning(disable : 4018) // '>=': signed/unsigned mismatch
#pragma warning(disable : 4267) // 'argument': conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable : 4267) // 'argument': conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable : 4244) // 'argument': conversion from 'int' to 'short', possible loss of data
#pragma warning(disable : 5202) // a global module fragment can only contain preprocessor directives

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma warning(pop)

export module external.implementaion;

export IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler_Custom(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}