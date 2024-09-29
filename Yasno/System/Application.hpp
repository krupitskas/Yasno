#pragma once

#include <memory>
#include <string>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wil/com.h>

namespace ysn
{

	class DxRenderer;
	class Window;
	class Game;
	class CommandQueue;

	class Application
	{
	public:
		static void Create(HINSTANCE hInst);

		static void Destroy();
		static Application& Get();

		bool IsTearingSupported() const;

		std::shared_ptr<Window> CreateRenderWindow(const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync = true);

		void DestroyWindow(const std::wstring& windowName);
		void DestroyWindow(std::shared_ptr<Window> window);

		std::shared_ptr<Window> GetWindowByName(const std::wstring& windowName);

		int Run(std::shared_ptr<Game> pGame);

		void Quit(int exitCode = 0);

		wil::com_ptr<ID3D12Device5> GetDevice() const;

		std::shared_ptr<CommandQueue> GetDirectQueue() const;
		std::shared_ptr<CommandQueue> GetComputeQueue() const;
		std::shared_ptr<CommandQueue> GetCopyQueue() const;

		std::shared_ptr<ysn::DxRenderer> GetRenderer() const;

		void Flush();

	protected:
		Application(HINSTANCE hInst);
		virtual ~Application();

	private:
		Application(const Application& copy) = delete;
		Application& operator=(const Application& other) = delete;

		HINSTANCE m_hInstance;

		std::shared_ptr<ysn::DxRenderer> m_dx_renderer;

		bool is_initialized = false;
	};

} // namespace ysn
