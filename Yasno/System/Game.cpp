#include "Game.hpp"

#include <cassert>

#include <DirectXMath.h>

#include "Application.hpp"
#include "Window.hpp"

namespace ysn
{

	Game::Game(const std::wstring& name, int width, int height, bool vsync) :
		m_Name(name), m_Width(width), m_Height(height), m_vsync(vsync)
	{}

	Game::~Game()
	{
		// assert(!m_pWindow && "Use Game::Destroy() before destruction.");
	}

	bool Game::Initialize()
	{
		// Check for DirectX Math library support.
		if (!DirectX::XMVerifyCPUSupport())
		{
			MessageBoxA(NULL, "Failed to verify DirectX Math library support.", "Error", MB_OK | MB_ICONERROR);
			return false;
		}

		// m_pWindow = Application::Get().CreateRenderWindow(m_Name, m_Width, m_Height, m_vSync);
		// m_pWindow->RegisterCallbacks(shared_from_this());
		// m_pWindow->Show();

		return true;
	}

	void Game::Destroy()
	{
		// Application::Get().DestroyWindow(m_pWindow);
		// m_pWindow.reset();
	}

	void Game::OnUpdate(UpdateEventArgs& /*e*/)
	{}

	void Game::OnRender(RenderEventArgs& /*e*/)
	{}

	void Game::OnResize(ResizeEventArgs& e)
	{
		m_Width = e.Width;
		m_Height = e.Height;
	}

	void Game::OnWindowDestroy()
	{
		// If the Window which we are registered to is
		// destroyed, then any resources which are associated
		// to the window must be released.
		UnloadContent();
	}

} // namespace ysn
