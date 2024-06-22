#include "GameInput.hpp"

namespace ysn
{
	void GameInput::Initialize(HWND window_handle)
	{
		keyboard = std::make_unique<DirectX::Keyboard>();
		mouse = std::make_unique<DirectX::Mouse>();
		mouse->SetWindow(window_handle);
	}

} // namespace ysn
