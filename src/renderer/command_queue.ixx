module;

#include <wil/com.h>
#include <d3d12.h>
#include <pix3.h>

export module renderer.command_queue;

import std;

import system.helpers;
import system.string_helpers;
import system.logger;
import renderer.dx_types;

export namespace ysn
{

class GraphicsCommandList
{
public:
    wil::com_ptr<DxGraphicsCommandList> list;
    //wil::com_ptr<ID3D12CommandAllocator> allocator;
};

class CommandQueue
{
public:
    CommandQueue(wil::com_ptr<ID3D12Device5> device, D3D12_COMMAND_LIST_TYPE type);
    virtual ~CommandQueue();

    GraphicsCommandList GetCommandList(std::string name);

    // Execute a command list.
    // Returns the fence value to wait for this command list.
    uint64_t ExecuteCommandList(GraphicsCommandList cmd_list);

    uint64_t Signal();
    bool IsFenceComplete(uint64_t fenceValue);
    void WaitForFenceValue(uint64_t fenceValue);
    void Flush();

    wil::com_ptr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

protected:
    wil::com_ptr<ID3D12CommandAllocator> CreateCommandAllocator();
    GraphicsCommandList CreateCommandList(wil::com_ptr<ID3D12CommandAllocator> allocator);

private:
    // Keep track of command allocators that are "in-flight"
    struct CommandAllocatorEntry
    {
        uint64_t fenceValue;
        wil::com_ptr<ID3D12CommandAllocator> cmd_allocator;
    };

    using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
    using CommandListQueue = std::queue<GraphicsCommandList>;

    D3D12_COMMAND_LIST_TYPE m_cmd_list_type;
    wil::com_ptr<ID3D12Device5> m_d3d12Device;
    wil::com_ptr<ID3D12CommandQueue> m_d3d12CommandQueue;
    wil::com_ptr<ID3D12Fence> m_d3d12Fence;
    HANDLE m_FenceEvent;
    uint64_t m_FenceValue;

    CommandAllocatorQueue m_CommandAllocatorQueue;
    CommandListQueue m_CommandListQueue;
};

} // namespace ysn

module :private;

namespace ysn
{

CommandQueue::CommandQueue(wil::com_ptr<ID3D12Device5> device, D3D12_COMMAND_LIST_TYPE type) :
    m_cmd_list_type(type), m_d3d12Device(device), m_FenceValue(0)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    // TODO: Exceptions in constructors are bad
    ThrowIfFailed(m_d3d12Device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_d3d12CommandQueue)));
    ThrowIfFailed(m_d3d12Device->CreateFence(m_FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_d3d12Fence)));

    m_FenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    // TODO: check asset!
}

CommandQueue::~CommandQueue()
{
}

uint64_t CommandQueue::Signal()
{
    uint64_t fenceValue = ++m_FenceValue;
    m_d3d12CommandQueue->Signal(m_d3d12Fence.get(), fenceValue);
    return fenceValue;
}

bool CommandQueue::IsFenceComplete(uint64_t fenceValue)
{
    return m_d3d12Fence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::WaitForFenceValue(uint64_t fenceValue)
{
    if (!IsFenceComplete(fenceValue))
    {
        m_d3d12Fence->SetEventOnCompletion(fenceValue, m_FenceEvent);
        ::WaitForSingleObject(m_FenceEvent, std::numeric_limits<uint32_t>::max());
    }
}

void CommandQueue::Flush()
{
    WaitForFenceValue(Signal());
}

wil::com_ptr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const
{
    return m_d3d12CommandQueue;
}

wil::com_ptr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator()
{
    wil::com_ptr<ID3D12CommandAllocator> cmd_allocator;

    if (HRESULT hr = m_d3d12Device->CreateCommandAllocator(m_cmd_list_type, IID_PPV_ARGS(&cmd_allocator)); hr != S_OK)
    {
        LogFatal << "Can't create command allocator: " << ConvertHrToString(hr) << " \n";
    }

    return cmd_allocator;
}

GraphicsCommandList CommandQueue::CreateCommandList(wil::com_ptr<ID3D12CommandAllocator> allocator)
{
    GraphicsCommandList gfx_cmd_list;

    if (HRESULT hr =
            m_d3d12Device->CreateCommandList(0, m_cmd_list_type, allocator.get(), nullptr, IID_PPV_ARGS(&gfx_cmd_list.list));
        hr != S_OK)
    {
        LogFatal << "Can't create command list: " << ConvertHrToString(hr) << " \n";
    }

    return gfx_cmd_list;
}

GraphicsCommandList CommandQueue::GetCommandList(std::string name)
{
    wil::com_ptr<ID3D12CommandAllocator> cmd_allocator;
    GraphicsCommandList cmd_list;

    if (!m_CommandAllocatorQueue.empty() && IsFenceComplete(m_CommandAllocatorQueue.front().fenceValue))
    {
        cmd_allocator = m_CommandAllocatorQueue.front().cmd_allocator;
        m_CommandAllocatorQueue.pop();

        ThrowIfFailed(cmd_allocator->Reset());
    }
    else
    {
        cmd_allocator = CreateCommandAllocator();
    }

    if (!m_CommandListQueue.empty())
    {
        cmd_list = m_CommandListQueue.front();
        m_CommandListQueue.pop();

        ThrowIfFailed(cmd_list.list->Reset(cmd_allocator.get(), nullptr));
    }
    else
    {
        cmd_list = CreateCommandList(cmd_allocator);
    }

#ifndef YSN_RELEASE
    cmd_list.list->SetName(std::wstring(name.begin(), name.end()).c_str());
#endif

    //cmd_list.allocator = cmd_allocator;

    // Associate the command allocator with the command list so that it can be
    // retrieved when the command list is executed.
    ThrowIfFailed(cmd_list.list->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), cmd_allocator.get()));

#ifndef YSN_RELEASE
    PIXBeginEvent(cmd_list.list.get(), PIX_COLOR_DEFAULT, name.c_str());
#endif

    return cmd_list;
}

uint64_t CommandQueue::ExecuteCommandList(GraphicsCommandList cmd_list)
{
#ifndef YSN_RELEASE
    PIXEndEvent(cmd_list.list.get());
#endif

    cmd_list.list->Close();

    ID3D12CommandList* const ppCommandLists[] = {cmd_list.list.get()};

    ID3D12CommandAllocator* cmd_allocator = nullptr;
    UINT dataSize = sizeof(ID3D12CommandAllocator);
    ThrowIfFailed(cmd_list.list->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &cmd_allocator));

    m_d3d12CommandQueue->ExecuteCommandLists(1, ppCommandLists);
    uint64_t fenceValue = Signal();

    m_CommandAllocatorQueue.emplace(CommandAllocatorEntry{fenceValue, cmd_allocator});
    m_CommandListQueue.push(cmd_list);

    // The ownership of the command allocator has been transferred to the ComPtr
    // in the command allocator queue. It is safe to release the reference
    // in this temporary COM pointer here.
    cmd_allocator->Release();

    return fenceValue;
}
} // namespace ysn
