module;

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wil/com.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <Keyboard.h>
#include <Mouse.h>
#include <dxcore.h>
#include <Windows.h>

#include <System/Helpers.hpp>

#include <resource.h>

export module system.application;

import std;
import renderer.dxtypes;
import renderer.dxrenderer;
import renderer.command_queue;
import system.window;
import system.logger;
import system.game;

export namespace ysn
{
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

		wil::com_ptr<DxDevice> GetDevice() const;

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

module :private;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace ysn
{
	constexpr wchar_t WINDOW_CLASS_NAME[] = L"yasno";

	using WindowPtr = std::shared_ptr<Window>;
	using WindowMap = std::map<HWND, WindowPtr>;
	using WindowNameMap = std::map<std::wstring, WindowPtr>;

	static ysn::Application* gs_pSingelton = nullptr;
	static WindowMap gs_Windows;
	static WindowNameMap gs_WindowByName;

	extern LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	// A wrapper struct to allow shared pointers for the window class.
	struct MakeWindow : public Window
	{
		MakeWindow(HWND hWnd, const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync) :
			Window(hWnd, windowName, clientWidth, clientHeight, vSync)
		{}
	};

	Application::Application(HINSTANCE hInst) : m_hInstance(hInst)
	{
		ysn::InitializeLogger();

		std::cout << "Starting application\n";

		// Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
		// Using this awareness context allows the client area of the window
		// to achieve 100% scaling while still allowing non-client window content to
		// be rendered in a DPI sensitive fashion.
		//SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

		// TODO(DPI): Bring awarness back later

		WNDCLASSEXW wndClass = { 0 };
		wndClass.cbSize = sizeof(WNDCLASSEX);
		wndClass.style = CS_HREDRAW | CS_VREDRAW;
		wndClass.lpfnWndProc = &WndProc;
		wndClass.hInstance = m_hInstance;
		wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wndClass.hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON1));
		wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wndClass.lpszMenuName = nullptr;
		wndClass.lpszClassName = WINDOW_CLASS_NAME;
		wndClass.hIconSm = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_ICON1));

		if (!RegisterClassExW(&wndClass))
		{
			MessageBoxA(NULL, "Unable to register the window class.", "Error", MB_OK | MB_ICONERROR);
		}

		m_dx_renderer = std::make_shared<ysn::DxRenderer>();

		if(!m_dx_renderer->Initialize())
		{
			std::cerr << "Can't initialize renderer\n";
			return;
		}

		is_initialized = true;
	}

	void Application::Create(HINSTANCE hInst)
	{
		if (!gs_pSingelton)
		{
			gs_pSingelton = new Application(hInst);
		}
	}

	Application& Application::Get()
	{
		assert(gs_pSingelton);
		return *gs_pSingelton;
	}

	void Application::Destroy()
	{
		if (gs_pSingelton)
		{
			assert(gs_Windows.empty() && gs_WindowByName.empty() && "All windows should be destroyed before destroying the application instance.");

			delete gs_pSingelton;
			gs_pSingelton = nullptr;

			std::cout << "Shutting down application\n";
		}
	}

	Application::~Application()
	{
		Flush();
	}

	bool Application::IsTearingSupported() const
	{
		return m_dx_renderer->IsTearingSupported();
	}

	std::shared_ptr<Window> Application::CreateRenderWindow(const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync)
	{
		// First check if a window with the given name already exists.
		WindowNameMap::iterator windowIter = gs_WindowByName.find(windowName);
		if (windowIter != gs_WindowByName.end())
		{
			return windowIter->second;
		}

		RECT windowRect = { 0, 0, clientWidth, clientHeight };
		AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

		//ImGui_ImplWin32_EnableDpiAwareness();

		HWND hWnd = CreateWindowW(WINDOW_CLASS_NAME, windowName.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr, nullptr, m_hInstance, nullptr);

		if (!hWnd)
		{
			MessageBoxA(NULL, "Could not create the render window.", "Error", MB_OK | MB_ICONERROR);
			return nullptr;
		}

		WindowPtr pWindow = std::make_shared<MakeWindow>(hWnd, windowName, clientWidth, clientHeight, vSync);

		gs_Windows.insert(WindowMap::value_type(hWnd, pWindow));
		gs_WindowByName.insert(WindowNameMap::value_type(windowName, pWindow));

		return pWindow;
	}

	void Application::DestroyWindow(std::shared_ptr<Window> window)
	{
		if (window)
			window->Destroy();
	}

	void Application::DestroyWindow(const std::wstring& windowName)
	{
		WindowPtr pWindow = GetWindowByName(windowName);
		if (pWindow)
		{
			DestroyWindow(pWindow);
		}
	}

	std::shared_ptr<Window> Application::GetWindowByName(const std::wstring& windowName)
	{
		std::shared_ptr<Window> window;
		WindowNameMap::iterator iter = gs_WindowByName.find(windowName);
		if (iter != gs_WindowByName.end())
		{
			window = iter->second;
		}

		return window;
	}

	int Application::Run(std::shared_ptr<Game> pGame)
	{
		if (!is_initialized)
			return -1;

		if (!pGame->Initialize())
			return 1;
		if (!pGame->LoadContent())
			return 2;

		MSG msg = { 0 };
		while (msg.message != WM_QUIT)
		{
			if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		// Flush any commands in the commands queues before quiting.
		Flush();

		pGame->UnloadContent();
		pGame->Destroy();

		return static_cast<int>(msg.wParam);
	}

	void Application::Quit(int exitCode)
	{
		PostQuitMessage(exitCode);
	}

	wil::com_ptr<DxDevice> Application::GetDevice() const
	{
		return m_dx_renderer->GetDevice();
	}

	std::shared_ptr<CommandQueue> Application::GetDirectQueue() const
	{
		return m_dx_renderer->GetDirectQueue();
	}

	std::shared_ptr<CommandQueue> Application::GetComputeQueue() const
	{
		return m_dx_renderer->GetComputeQueue();
	}

	std::shared_ptr<CommandQueue> Application::GetCopyQueue() const
	{
		return m_dx_renderer->GetCopyQueue();
	}

	void Application::Flush()
	{
		m_dx_renderer->FlushQueues();
	}

	std::shared_ptr<ysn::DxRenderer> Application::GetRenderer() const
	{
		return m_dx_renderer;
	}

	// Remove a window from our window lists.
	static void RemoveWindow(HWND hWnd)
	{
		WindowMap::iterator windowIter = gs_Windows.find(hWnd);
		if (windowIter != gs_Windows.end())
		{
			WindowPtr pWindow = windowIter->second;
			gs_WindowByName.erase(pWindow->GetWindowName());
			gs_Windows.erase(windowIter);
		}
	}

	extern LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		WindowPtr pWindow;
		{
			WindowMap::iterator iter = gs_Windows.find(hwnd);
			if (iter != gs_Windows.end())
			{
				pWindow = iter->second;
			}
		}

		if (pWindow)
		{
			// TODO(imgui)
			if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
				return true;

			switch (message)
			{
				case WM_PAINT:
			{
					UpdateEventArgs updateEventArgs(0.0f, 0.0f);
					pWindow->OnUpdate(updateEventArgs);
					RenderEventArgs renderEventArgs(0.0f, 0.0f);
					// Delta time will be filled in by the Window.
					pWindow->OnRender(renderEventArgs);
				}
			break;
				case WM_ACTIVATEAPP:
					DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
					DirectX::Mouse::ProcessMessage(message, wParam, lParam);
					break;

					// Mouse below
				case WM_ACTIVATE:
				case WM_INPUT:
				case WM_MOUSEMOVE:
				case WM_LBUTTONDOWN:
				case WM_LBUTTONUP:
				case WM_RBUTTONDOWN:
				case WM_RBUTTONUP:
				case WM_MBUTTONDOWN:
				case WM_MBUTTONUP:
				case WM_MOUSEWHEEL:
				case WM_XBUTTONDOWN:
				case WM_XBUTTONUP:
				case WM_MOUSEHOVER:
					DirectX::Mouse::ProcessMessage(message, wParam, lParam);
					break;

					// Keyboard below
				case WM_KEYDOWN:
				case WM_KEYUP:
				case WM_SYSKEYUP:
				case WM_SYSKEYDOWN:
					DirectX::Keyboard::ProcessMessage(message, wParam, lParam);
					break;

				case WM_SIZE:
					{
					int width = ((int)(short)LOWORD(lParam));
					int height = ((int)(short)HIWORD(lParam));

					ResizeEventArgs resizeEventArgs(width, height);
					pWindow->OnResize(resizeEventArgs);
				}
					break;
				case WM_DESTROY:
					{
					// If a window is being destroyed, remove it from the
					// window maps.
					RemoveWindow(hwnd);

					if (gs_Windows.empty())
					{
						// If there are no more windows, quit the application.
						PostQuitMessage(0);
					}
				}
					break;
				default:
					return DefWindowProcW(hwnd, message, wParam, lParam);
			}
		}
		else
		{
			return DefWindowProcW(hwnd, message, wParam, lParam);
		}

		return 0;
	}
}

