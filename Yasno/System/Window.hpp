#pragma once

#include <array>

#include <d3d12.h>
#include <dxgi1_5.h>
#include <wil/com.h>

#include <System/Events.hpp>
#include <System/HighResolutionClock.hpp>
#include <Renderer/DescriptorHeap.hpp>

namespace ysn
{

	class Game;

	class Window
	{
	public:
		static constexpr UINT BufferCount = 3;

		HWND GetWindowHandle() const;

		void Destroy();

		const std::wstring& GetWindowName() const;

		int GetClientWidth() const;
		int GetClientHeight() const;

		bool IsVSync() const;
		void SetVSync(bool vSync);
		void ToggleVSync();

		bool IsFullScreen() const;

		void SetFullscreen(bool fullscreen);
		void ToggleFullscreen();

		void Show();

		void Hide();

		UINT GetCurrentBackBufferIndex() const;

		UINT Present();

		D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferUAV() const;

		wil::com_ptr<ID3D12Resource> GetCurrentBackBuffer() const;

		Window(const Window& copy) = delete;
		Window& operator=(const Window& other) = delete;
		Window() = delete;

		void HideAndCaptureCursor();
		void ShowAndReleaseCursor();

		double GetDeltaTime()
		{
			return m_UpdateClock.GetDeltaMilliseconds();
		}

		// Register a Game with this window. This allows
		// the window to callback functions in the Game class.
		void RegisterCallbacks(std::shared_ptr<Game> pGame);

	protected:
		friend LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

		// Only the application can create a window.
		friend class Application;
		// The DirectXTemplate class needs to register itself with a window.
		friend class Game;

		Window(HWND HWnd, const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync);
		virtual ~Window();

		// Update and Draw can only be called by the application.
		virtual void OnUpdate(UpdateEventArgs& e);
		virtual void OnRender(RenderEventArgs& e);

		// The window was resized.
		virtual void OnResize(ResizeEventArgs& e);

		// Create the swapchain.
		wil::com_ptr<IDXGISwapChain4> CreateSwapChain();

		// Update the render target views for the swapchain back buffers.
		void UpdateRenderTargetViews();

	private:
		HWND m_hWnd;

		std::wstring m_WindowName;

		int m_ClientWidth;
		int m_ClientHeight;
		bool m_VSync;
		bool m_Fullscreen;
		bool m_IsCursorHidden = false;

		HighResolutionClock m_UpdateClock;
		HighResolutionClock m_RenderClock;
		uint64_t m_FrameCounter;

		std::weak_ptr<Game> m_pGame;

		wil::com_ptr<IDXGISwapChain4> m_DxgiSwapChain;
		//wil::com_ptr<ID3D12DescriptorHeap> m_RtvDescriptorHeap;
		//wil::com_ptr<ID3D12DescriptorHeap> m_UavDescriptorHeap;
		wil::com_ptr<ID3D12Resource> m_BackBuffers[BufferCount];

		std::array<ysn::DescriptorHandle, BufferCount> m_rtv_descriptor_handles;
		std::array<ysn::DescriptorHandle, BufferCount> m_uav_descriptor_handles;

		//UINT m_RtvDescriptorSize;
		//UINT m_UavDescriptorSize;
		UINT m_CurrentBackBufferIndex;

		RECT m_WindowRect;
		bool m_IsTearingSupported;
	};

}
