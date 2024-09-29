#pragma once

#include <Windows.h>

#include <Keyboard.h>
#include <Mouse.h>

namespace ysn
{
	struct GameInput
	{
		void Initialize(HWND window_handle);

		std::unique_ptr<DirectX::Keyboard> keyboard;
		std::unique_ptr<DirectX::Mouse> mouse;
	};
}
