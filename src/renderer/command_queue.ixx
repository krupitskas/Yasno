module;

#include <wil/com.h>
#include <d3d12.h>
#include <pix3.h>

export module renderer.command_queue;

import std;

import system.helpers;
import system.string_helpers;
import system.logger;
import system.compilation;
import renderer.dx_types;

export namespace ysn
{
	class GraphicsCommandList
	{
	public:
		wil::com_ptr<DxGraphicsCommandList> list;
		// wil::com_ptr<ID3D12CommandAllocator> allocator;
	};

	class CommandQueue
	{
	public:
		CommandQueue(wil::com_ptr<ID3D12Device5> device, D3D12_COMMAND_LIST_TYPE type);
		virtual ~CommandQueue();

		bool Initialize();

		std::optional<GraphicsCommandList> GetCommandList(std::string name);
		std::optional<uint64_t> ExecuteCommandList(GraphicsCommandList cmd_list);
		std::optional<uint64_t> Signal();

		bool IsFenceComplete(uint64_t fence_value);
		void WaitForFenceValue(uint64_t fence_value);
		bool Flush();

		wil::com_ptr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

	protected:
		std::optional<wil::com_ptr<ID3D12CommandAllocator>> CreateCommandAllocator();
		GraphicsCommandList CreateCommandList(wil::com_ptr<ID3D12CommandAllocator> allocator);

	private:
		// Keep track of command allocators that are "in-flight"
		struct CommandAllocatorEntry
		{
			uint64_t fence_value;
			wil::com_ptr<ID3D12CommandAllocator> cmd_allocator;
		};

		using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
		using CommandListQueue = std::queue<GraphicsCommandList>;

		D3D12_COMMAND_LIST_TYPE m_cmd_list_type;
		wil::com_ptr<ID3D12Device5> m_device;
		wil::com_ptr<ID3D12CommandQueue> m_command_queue;
		wil::com_ptr<ID3D12Fence> m_fence;
		HANDLE m_fence_event;
		uint64_t m_fence_value;

		CommandAllocatorQueue m_cmd_allocator_queue;
		CommandListQueue m_cmd_list_queue;
	};

}

module :private;

namespace ysn
{

	CommandQueue::CommandQueue(wil::com_ptr<ID3D12Device5> device, D3D12_COMMAND_LIST_TYPE type) :
		m_cmd_list_type(type), m_device(device), m_fence_value(0)
	{
	}

	CommandQueue::~CommandQueue()
	{
	}

	std::optional<uint64_t> CommandQueue::Signal()
	{
		uint64_t fence_value = ++m_fence_value;
		if (auto result = m_command_queue->Signal(m_fence.get(), fence_value); result != S_OK)
		{
			LogFatal << "Can't signal in command queue: " << ConvertHrToString(result) << " \n";
			return std::nullopt;
		}
		return fence_value;
	}

	bool CommandQueue::IsFenceComplete(uint64_t fence_value)
	{
		return m_fence->GetCompletedValue() >= fence_value;
	}

	void CommandQueue::WaitForFenceValue(uint64_t fence_value)
	{
		if (!IsFenceComplete(fence_value))
		{
			m_fence->SetEventOnCompletion(fence_value, m_fence_event);
			::WaitForSingleObject(m_fence_event, std::numeric_limits<uint32_t>::max());
		}
	}

	bool CommandQueue::Flush()
	{
		auto signal_result = Signal();

		if (!signal_result.has_value())
		{
			return false;
		}

		WaitForFenceValue(signal_result.value());

		return true;
	}

	wil::com_ptr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const
	{
		return m_command_queue;
	}

	std::optional<wil::com_ptr<ID3D12CommandAllocator>> CommandQueue::CreateCommandAllocator()
	{
		wil::com_ptr<ID3D12CommandAllocator> cmd_allocator;

		if (HRESULT hr = m_device->CreateCommandAllocator(m_cmd_list_type, IID_PPV_ARGS(&cmd_allocator)); hr != S_OK)
		{
			LogFatal << "Can't create command allocator: " << ConvertHrToString(hr) << " \n";
			return std::nullopt;
		}

		return cmd_allocator;
	}

	GraphicsCommandList CommandQueue::CreateCommandList(wil::com_ptr<ID3D12CommandAllocator> allocator)
	{
		GraphicsCommandList gfx_cmd_list;

		if (HRESULT hr = m_device->CreateCommandList(0, m_cmd_list_type, allocator.get(), nullptr, IID_PPV_ARGS(&gfx_cmd_list.list));
			hr != S_OK)
		{
			LogFatal << "Can't create command list: " << ConvertHrToString(hr) << " \n";
		}

		return gfx_cmd_list;
	}

	bool CommandQueue::Initialize()
	{
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type = m_cmd_list_type;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;

		HRESULT result = m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_command_queue));

		if (result != S_OK)
		{
			LogFatal << "Can't create command queue: " << ConvertHrToString(result) << "\n";
			return false;
		}

		result = m_device->CreateFence(m_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));

		if (result != S_OK)
		{
			LogFatal << "Can't create fence: " << ConvertHrToString(result) << "\n";
			return false;
		}

		m_fence_event = ::CreateEvent(NULL, FALSE, FALSE, NULL);

		return true;
	}

	std::optional<GraphicsCommandList> CommandQueue::GetCommandList(std::string name)
	{
		wil::com_ptr<ID3D12CommandAllocator> cmd_allocator;
		GraphicsCommandList cmd_list;

		if (!m_cmd_allocator_queue.empty() && IsFenceComplete(m_cmd_allocator_queue.front().fence_value))
		{
			cmd_allocator = m_cmd_allocator_queue.front().cmd_allocator;
			m_cmd_allocator_queue.pop();

			HRESULT result = cmd_allocator->Reset();

			if (result != S_OK)
			{
				LogFatal << "Can't reset command allocator: " << ConvertHrToString(result) << "\n";
				return std::nullopt;
			}
		}
		else
		{
			auto new_cmd_allocator = CreateCommandAllocator();

			if (!new_cmd_allocator.has_value() != S_OK)
			{
				LogFatal << "Can't create new command allocator\n";
				return std::nullopt;
			}

			cmd_allocator = new_cmd_allocator.value();
		}

		if (!m_cmd_list_queue.empty())
		{
			cmd_list = m_cmd_list_queue.front();
			m_cmd_list_queue.pop();

			HRESULT result = cmd_list.list->Reset(cmd_allocator.get(), nullptr);

			if (result != S_OK)
			{
				LogFatal << "Can't reset command list: " << ConvertHrToString(result) << "\n";
				return std::nullopt;
			}
		}
		else
		{
			cmd_list = CreateCommandList(cmd_allocator);
		}

		if (auto result = cmd_list.list->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), cmd_allocator.get()); result != S_OK)
		{
			LogFatal << "Can't write allocator into cmd list: " << ConvertHrToString(result) << "\n";
			return std::nullopt;
		}

		if constexpr (!IsReleaseActive())
		{
			cmd_list.list->SetName(std::wstring(name.begin(), name.end()).c_str());
			PIXBeginEvent(cmd_list.list.get(), PIX_COLOR_DEFAULT, name.c_str());
		}

		return cmd_list;
	}

	std::optional<uint64_t> CommandQueue::ExecuteCommandList(GraphicsCommandList cmd_list)
	{
		if constexpr (!IsReleaseActive())
		{
			PIXEndEvent(cmd_list.list.get());
		}

		cmd_list.list->Close();

		ID3D12CommandList* const ppCommandLists[] = { cmd_list.list.get() };

		ID3D12CommandAllocator* cmd_allocator = nullptr;
		UINT dataSize = sizeof(ID3D12CommandAllocator);

		if (auto result = cmd_list.list->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &cmd_allocator); result != S_OK)
		{
			LogError << "Can't retrieve cmd allocator from cmd list: " << ConvertHrToString(result) << "\n";
			return std::nullopt;
		}

		m_command_queue->ExecuteCommandLists(1, ppCommandLists);

		const auto signal_result = Signal();

		if (!signal_result.has_value())
		{
			return std::nullopt;
		}

		m_cmd_allocator_queue.emplace(CommandAllocatorEntry{ signal_result.value(), cmd_allocator });
		m_cmd_list_queue.push(cmd_list);

		cmd_allocator->Release();

		return signal_result.value();
	}
}
