#pragma once

#include <memory>
#include <string>

#include "Events.hpp"

namespace ysn
{

	class Window;

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
