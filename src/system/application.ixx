module;

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wil/com.h>
#include <imgui.h>
#include <Keyboard.h>
#include <Mouse.h>
#include <dxcore.h>
#include <Windows.h>
#include <DirectXMath.h>

#include <resource.h>

export module system.application;


import std;
import renderer.dx_types;
import renderer.dxrenderer;
import renderer.descriptor_heap;
import renderer.command_queue;
import system.logger;
import system.events;
import system.clock;
import system.helpers;
import system.asserts;
import external.implementaion;

export namespace ysn
{
// todo: merge everything.
class Game;
class Window;

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

    friend class Application;
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
    wil::com_ptr<ID3D12Resource> m_BackBuffers[BufferCount];

    std::array<ysn::DescriptorHandle, BufferCount> m_rtv_descriptor_handles;
    std::array<ysn::DescriptorHandle, BufferCount> m_uav_descriptor_handles;

    UINT m_CurrentBackBufferIndex;

    RECT m_WindowRect;
    bool m_IsTearingSupported;
};

} // namespace ysn

module :private;

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
    {
    }
};

Application::Application(HINSTANCE hInst) : m_hInstance(hInst)
{
    ysn::InitializeLogger();

    LogInfo << "Starting application\n";

    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window
    // to achieve 100% scaling while still allowing non-client window content to
    // be rendered in a DPI sensitive fashion.
    // SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // TODO(DPI): Bring awarness back later

    WNDCLASSEXW wndClass = {0};
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

    if (!m_dx_renderer->Initialize())
    {
        LogError << "Can't initialize renderer\n";
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

        LogInfo << "Shutting down application\n";
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

    RECT windowRect = {0, 0, clientWidth, clientHeight};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // ImGui_ImplWin32_EnableDpiAwareness();

    HWND hWnd = CreateWindowW(
        WINDOW_CLASS_NAME,
        windowName.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        m_hInstance,
        nullptr);

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

    MSG msg = {0};
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
        if (ImGui_ImplWin32_WndProcHandler_Custom(hwnd, message, wParam, lParam))
        {
            return true;
        }

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
    swapChainDesc.SampleDesc = {1, 0};
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
    ThrowIfFailed(m_DxgiSwapChain->Present(syncInterval, presentFlags));
    m_CurrentBackBufferIndex = m_DxgiSwapChain->GetCurrentBackBufferIndex();

    return m_CurrentBackBufferIndex;
}

void Window::HideAndCaptureCursor()
{
    if (m_IsCursorHidden)
        return;

    m_IsCursorHidden = true;
}

void Window::ShowAndReleaseCursor()
{
    if (!m_IsCursorHidden)
        return;

    ShowCursor(TRUE);

    m_IsCursorHidden = false;
}

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
} // namespace ysn
