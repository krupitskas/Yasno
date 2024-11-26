#include "Window.hpp"

#include <cassert>
#include <algorithm>

#include <d3dx12.h>

#include <System/Application.hpp>
#include <Renderer/CommandQueue.hpp>
#include <System/Game.hpp>
#include <System/Helpers.hpp>
#include <Renderer/DxRenderer.hpp>

namespace ysn
{

	Window::Window(HWND HWnd, const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync) :
		m_hWnd(HWnd),
		m_WindowName(windowName),
		m_ClientWidth(clientWidth),
		m_ClientHeight(clientHeight),
		m_VSync(vSync),
		m_Fullscreen(false),
		m_FrameCounter(0)
	{
		Application& app = Application::Get();
		auto renderer = Application::Get().GetRenderer();

		m_IsTearingSupported = app.IsTearingSupported();

		m_DxgiSwapChain = CreateSwapChain();

		for (int i = 0; i < BufferCount; i++)
		{
			m_rtv_descriptor_handles[i] = renderer->GetRtvDescriptorHeap()->GetNewHandle();
			m_uav_descriptor_handles[i] = renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
		}

		UpdateRenderTargetViews();
	}

	Window::~Window()
	{
		// Window should be destroyed with Application::DestroyWindow before
		// the window goes out of scope.
		assert(!m_hWnd && "Use Application::DestroyWindow before destruction.");
	}

	HWND Window::GetWindowHandle() const
	{
		return m_hWnd;
	}

	const std::wstring& Window::GetWindowName() const
	{
		return m_WindowName;
	}

	void Window::Show()
	{
		::ShowWindow(m_hWnd, SW_SHOW);
	}

	/**
 * Hide the window.
 */
	void Window::Hide()
	{
		::ShowWindow(m_hWnd, SW_HIDE);
	}

	void Window::Destroy()
	{
		if (auto pGame = m_pGame.lock())
		{
			// Notify the registered game that the window is being destroyed.
			pGame->OnWindowDestroy();
		}
		if (m_hWnd)
		{
			DestroyWindow(m_hWnd);
			m_hWnd = nullptr;
		}
	}

	int Window::GetClientWidth() const
	{
		return m_ClientWidth;
	}

	int Window::GetClientHeight() const
	{
		return m_ClientHeight;
	}

	bool Window::IsVSync() const
	{
		return m_VSync;
	}

	void Window::SetVSync(bool vSync)
	{
		m_VSync = vSync;
	}

	void Window::ToggleVSync()
	{
		SetVSync(!m_VSync);
	}

	bool Window::IsFullScreen() const
	{
		return m_Fullscreen;
	}

	// Set the fullscreen state of the window.
	void Window::SetFullscreen(bool fullscreen)
	{
		if (m_Fullscreen != fullscreen)
		{
			m_Fullscreen = fullscreen;

			if (m_Fullscreen) // Switching to fullscreen.
			{
				// Store the current window dimensions so they can be restored
				// when switching out of fullscreen state.
				::GetWindowRect(m_hWnd, &m_WindowRect);

				// Set the window style to a borderless window so the client area fills
				// the entire screen.
				UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

				::SetWindowLongW(m_hWnd, GWL_STYLE, windowStyle);

				// Query the name of the nearest display device for the window.
				// This is required to set the fullscreen dimensions of the window
				// when using a multi-monitor setup.
				HMONITOR hMonitor = ::MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
				MONITORINFOEX monitorInfo = {};
				monitorInfo.cbSize = sizeof(MONITORINFOEX);
				::GetMonitorInfo(hMonitor, &monitorInfo);

				::SetWindowPos(
					m_hWnd,
					HWND_TOPMOST,
					monitorInfo.rcMonitor.left,
					monitorInfo.rcMonitor.top,
					monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
					monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
					SWP_FRAMECHANGED | SWP_NOACTIVATE);

				::ShowWindow(m_hWnd, SW_MAXIMIZE);
			}
			else
			{
				// Restore all the window decorators.
				::SetWindowLong(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

				::SetWindowPos(
					m_hWnd,
					HWND_NOTOPMOST,
					m_WindowRect.left,
					m_WindowRect.top,
					m_WindowRect.right - m_WindowRect.left,
					m_WindowRect.bottom - m_WindowRect.top,
					SWP_FRAMECHANGED | SWP_NOACTIVATE);

				::ShowWindow(m_hWnd, SW_NORMAL);
			}
		}
	}

	void Window::ToggleFullscreen()
	{
		SetFullscreen(!m_Fullscreen);
	}

	void Window::RegisterCallbacks(std::shared_ptr<Game> pGame)
	{
		m_pGame = pGame;

		return;
	}

	void Window::OnUpdate(UpdateEventArgs&)
	{
		m_UpdateClock.Tick();

		if (auto pGame = m_pGame.lock())
		{
			m_FrameCounter++;

			UpdateEventArgs updateEventArgs(m_UpdateClock.GetDeltaSeconds(), m_UpdateClock.GetTotalSeconds());
			pGame->OnUpdate(updateEventArgs);
		}
	}

	void Window::OnRender(RenderEventArgs&)
	{
		m_RenderClock.Tick();

		if (auto pGame = m_pGame.lock())
		{
			RenderEventArgs renderEventArgs(m_RenderClock.GetDeltaSeconds(), m_RenderClock.GetTotalSeconds());
			pGame->OnRender(renderEventArgs);
		}
	}

	void Window::OnResize(ResizeEventArgs& e)
	{
		// Update the client size.
		if (m_ClientWidth != e.Width || m_ClientHeight != e.Height)
		{
			m_ClientWidth = std::max(1, e.Width);
			m_ClientHeight = std::max(1, e.Height);

			Application::Get().Flush();

			for (int i = 0; i < BufferCount; ++i)
			{
				m_BackBuffers[i].reset();
			}

			DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
			ThrowIfFailed(m_DxgiSwapChain->GetDesc(&swapChainDesc));
			ThrowIfFailed(m_DxgiSwapChain->ResizeBuffers(
				BufferCount, m_ClientWidth, m_ClientHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

			m_CurrentBackBufferIndex = m_DxgiSwapChain->GetCurrentBackBufferIndex();

			UpdateRenderTargetViews();
		}

		if (auto pGame = m_pGame.lock())
		{
			pGame->OnResize(e);
		}
	}

	wil::com_ptr<IDXGISwapChain4> Window::CreateSwapChain()
	{
		Application& app = Application::Get();

		wil::com_ptr<IDXGISwapChain4> dxgiSwapChain4;
		wil::com_ptr<IDXGIFactory4> dxgiFactory4;
		UINT createFactoryFlags = 0;
	#if defined(_DEBUG)
		createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
	#endif

		ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = m_ClientWidth;
		swapChainDesc.Height = m_ClientHeight;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // TODO: Unify format
		swapChainDesc.Stereo = FALSE;
		swapChainDesc.SampleDesc = { 1, 0 };
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = BufferCount;
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		// It is recommended to always allow tearing if tearing support is available.
		swapChainDesc.Flags = m_IsTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
		ID3D12CommandQueue* pCommandQueue = app.GetDirectQueue()->GetD3D12CommandQueue().get();

		wil::com_ptr<IDXGISwapChain1> swapChain1;
		ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(pCommandQueue, m_hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1));

		// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
		// will be handled manually.
		ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER));

		dxgiSwapChain4 = swapChain1.try_query<IDXGISwapChain4>();

		if (!dxgiSwapChain4)
		{
			// TODO: handle it right
			ThrowIfFailed(1L);
		}

		m_CurrentBackBufferIndex = dxgiSwapChain4->GetCurrentBackBufferIndex();

		return dxgiSwapChain4;
	}

	// Update the render target views for the swapchain back buffers.
	void Window::UpdateRenderTargetViews()
	{
		auto device = Application::Get().GetDevice();
		auto renderer = Application::Get().GetRenderer();

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		for (int i = 0; i < BufferCount; i++)
		{
			// TODO: Check out R10G10B10A2 here
			wil::com_ptr<ID3D12Resource> backBuffer;

			ThrowIfFailed(m_DxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

			backBuffer->SetName(L"BackBuffer");

			device->CreateRenderTargetView(backBuffer.get(), nullptr, m_rtv_descriptor_handles[i].cpu);

			m_BackBuffers[i] = backBuffer;
		}
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Window::GetCurrentRenderTargetView() const
	{
		return m_rtv_descriptor_handles[m_CurrentBackBufferIndex].cpu;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Window::GetCurrentBackBufferUAV() const
	{
		return m_uav_descriptor_handles[m_CurrentBackBufferIndex].cpu;
	}

	wil::com_ptr<ID3D12Resource> Window::GetCurrentBackBuffer() const
	{
		return m_BackBuffers[m_CurrentBackBufferIndex];
	}

	UINT Window::GetCurrentBackBufferIndex() const
	{
		return m_CurrentBackBufferIndex;
	}

	UINT Window::Present()
	{
		UINT syncInterval = m_VSync ? 1 : 0;
		UINT presentFlags = m_IsTearingSupported && !m_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;

		//DXGI_ERROR_DEVICE_RESET
		//DXGI_ERROR_DEVICE_REMOVED

		ThrowIfFailed(m_DxgiSwapChain->Present(syncInterval, presentFlags));
		m_CurrentBackBufferIndex = m_DxgiSwapChain->GetCurrentBackBufferIndex();

		return m_CurrentBackBufferIndex;
	}

	void Window::HideAndCaptureCursor()
	{
		if (m_IsCursorHidden)
			return;

		// RECT rect;
		// GetClientRect(m_hWnd, &rect);
		// MapWindowPoints(m_hWnd, nullptr, reinterpret_cast<POINT*>(&rect), 2);
		// ClipCursor(&rect);
		// ShowCursor(FALSE);
		// SetCapture(m_hWnd);

		m_IsCursorHidden = true;
	}

	void Window::ShowAndReleaseCursor()
	{
		if (!m_IsCursorHidden)
			return;

		ShowCursor(TRUE);
		// ClipCursor(nullptr);
		// ReleaseCapture();

		m_IsCursorHidden = false;
	}

} // namespace ysn
