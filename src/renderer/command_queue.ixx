module;

#include <wil/com.h>
#include <d3d12.h>
#include <pix3.h>

export module renderer.command_queue;

import std;

import system.helpers;

export namespace ysn
{
class CommandQueue
{
public:
    CommandQueue(wil::com_ptr<ID3D12Device5> device, D3D12_COMMAND_LIST_TYPE type);
    virtual ~CommandQueue();

    wil::com_ptr<ID3D12GraphicsCommandList4> GetCommandList(std::string name);

    // Execute a command list.
    // Returns the fence value to wait for this command list.
    uint64_t ExecuteCommandList(wil::com_ptr<ID3D12GraphicsCommandList4> commandList);

    uint64_t Signal();
    bool IsFenceComplete(uint64_t fenceValue);
    void WaitForFenceValue(uint64_t fenceValue);
    void Flush();

    wil::com_ptr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

protected:
    wil::com_ptr<ID3D12CommandAllocator> CreateCommandAllocator();
    wil::com_ptr<ID3D12GraphicsCommandList4> CreateCommandList(wil::com_ptr<ID3D12CommandAllocator> allocator);

private:
    // Keep track of command allocators that are "in-flight"
    struct CommandAllocatorEntry
    {
        uint64_t fenceValue;
        wil::com_ptr<ID3D12CommandAllocator> commandAllocator;
    };

    using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
    using CommandListQueue = std::queue<wil::com_ptr<ID3D12GraphicsCommandList4>>;

    D3D12_COMMAND_LIST_TYPE m_CommandListType;
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
    m_CommandListType(type), m_d3d12Device(device), m_FenceValue(0)
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
    wil::com_ptr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(m_d3d12Device->CreateCommandAllocator(m_CommandListType, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
}

wil::com_ptr<ID3D12GraphicsCommandList4> CommandQueue::CreateCommandList(wil::com_ptr<ID3D12CommandAllocator> allocator)
{
    wil::com_ptr<ID3D12GraphicsCommandList4> commandList;
    ThrowIfFailed(m_d3d12Device->CreateCommandList(0, m_CommandListType, allocator.get(), nullptr, IID_PPV_ARGS(&commandList)));

    return commandList;
}

wil::com_ptr<ID3D12GraphicsCommandList4> CommandQueue::GetCommandList(std::string name)
{
    wil::com_ptr<ID3D12CommandAllocator> commandAllocator;
    wil::com_ptr<ID3D12GraphicsCommandList4> commandList;

    if (!m_CommandAllocatorQueue.empty() && IsFenceComplete(m_CommandAllocatorQueue.front().fenceValue))
    {
        commandAllocator = m_CommandAllocatorQueue.front().commandAllocator;
        m_CommandAllocatorQueue.pop();

        ThrowIfFailed(commandAllocator->Reset());
    }
    else
    {
        commandAllocator = CreateCommandAllocator();
    }

    if (!m_CommandListQueue.empty())
    {
        commandList = m_CommandListQueue.front();
        m_CommandListQueue.pop();

        ThrowIfFailed(commandList->Reset(commandAllocator.get(), nullptr));
    }
    else
    {
        commandList = CreateCommandList(commandAllocator);
    }

#ifndef YSN_RELEASE
    commandList->SetName(std::wstring(name.begin(), name.end()).c_str());
#endif

    // Associate the command allocator with the command list so that it can be
    // retrieved when the command list is executed.
    ThrowIfFailed(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.get()));

#ifndef YSN_RELEASE
    PIXBeginEvent(commandList.get(), PIX_COLOR_DEFAULT, name.c_str());
#endif

    return commandList;
}

uint64_t CommandQueue::ExecuteCommandList(wil::com_ptr<ID3D12GraphicsCommandList4> commandList)
{
#ifndef YSN_RELEASE
    PIXEndEvent(commandList.get());
#endif

    commandList->Close();

    ID3D12CommandAllocator* commandAllocator;
    UINT dataSize = sizeof(commandAllocator);
    ThrowIfFailed(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));

    ID3D12CommandList* const ppCommandLists[] = {commandList.get()};

    m_d3d12CommandQueue->ExecuteCommandLists(1, ppCommandLists);
    uint64_t fenceValue = Signal();

    m_CommandAllocatorQueue.emplace(CommandAllocatorEntry{fenceValue, commandAllocator});
    m_CommandListQueue.push(commandList);

    // The ownership of the command allocator has been transferred to the ComPtr
    // in the command allocator queue. It is safe to release the reference
    // in this temporary COM pointer here.
    commandAllocator->Release();

    return fenceValue;
}
} // namespace ysn
