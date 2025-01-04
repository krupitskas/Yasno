module;

#include <Windows.h>

#include <Keyboard.h>
#include <Mouse.h>

export module system.game_input;

export namespace ysn
{
	struct GameInput
	{
		void Initialize(HWND window_handle);

		std::unique_ptr<DirectX::Keyboard> keyboard;
		std::unique_ptr<DirectX::Mouse> mouse;
	};
}

module :private;

namespace ysn
{
	void GameInput::Initialize(HWND window_handle)
	{
		keyboard = std::make_unique<DirectX::Keyboard>();
		mouse = std::make_unique<DirectX::Mouse>();
		mouse->SetWindow(window_handle);
	}
}
