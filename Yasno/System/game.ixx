module;

#include <Windows.h>
#include <DirectXMath.h>

export module system.game;

import std;
import system.window;
import system.events;
import system.application;

export namespace ysn
{
	class Game : std::enable_shared_from_this<Game>
	{
	public:
		Game(const std::wstring& name, int width, int height, bool vSync);
		virtual ~Game();

		int GetClientWidth() const
		{
			return m_Width;
		}

		int GetClientHeight() const
		{
			return m_Height;
		}

		virtual bool Initialize();

		virtual bool LoadContent() = 0;

		virtual void UnloadContent() = 0;

		virtual void Destroy();

		void SetWindow(std::shared_ptr<Window> window)
		{
			m_pWindow = window;
		}

	protected:
		friend class Window;

		virtual void OnUpdate(UpdateEventArgs& e);

		virtual void OnRender(RenderEventArgs& e);

		virtual void OnResize(ResizeEventArgs& e);

		virtual void OnWindowDestroy();

		std::shared_ptr<Window> m_pWindow;

		std::wstring m_Name;
		int m_Width;
		int m_Height;
		int m_frame_number = 0;
		int m_rtx_frames_accumulated = 0;
		bool m_vsync;
	};
}

module :private;

namespace ysn
{
	Game::Game(const std::wstring& name, int width, int height, bool vsync) :
		m_Name(name), m_Width(width), m_Height(height), m_vsync(vsync)
	{
	}

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
	{
	}

	void Game::OnRender(RenderEventArgs& /*e*/)
	{
	}

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
}
