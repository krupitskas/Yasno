#pragma once

#include <wil/com.h>
#include <d3d12.h>

#include <cstdint>
#include <queue>

namespace ysn
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

}
