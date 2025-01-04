module;

#include <wil/com.h>
#include <d3d12.h>
#include <d3dx12.h>

export module renderer.descriptor_heap;

import renderer.dx_types;
import system.helpers;
import system.compilation;
import system.logger;
import system.string_helpers;

export namespace ysn
{
// TODO: make it typesafe and check if it's the same heap
	struct DescriptorHandle
	{
		uint32_t index = 0;
		CD3DX12_CPU_DESCRIPTOR_HANDLE cpu;
		CD3DX12_GPU_DESCRIPTOR_HANDLE gpu;
	};

	class DescriptorHeap
	{
	public:
		DescriptorHeap(wil::com_ptr<DxDevice> d3d12device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count, bool shader_visible = false);

		bool Initialize();

		DescriptorHandle GetNewHandle();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(DescriptorHandle handle);
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(DescriptorHandle handle);

		uint32_t GetDescriptorIndex(const DescriptorHandle& descriptorHandle) const
		{
			const uint32_t handle_ptr = (uint32_t)descriptorHandle.gpu.ptr;
			const uint32_t heap_start_ptr = uint32_t(m_descriptor_heap->GetGPUDescriptorHandleForHeapStart().ptr);
			const uint32_t increment = GetDescriptorHandleIncrementSize();
			return static_cast<uint32_t>((handle_ptr - heap_start_ptr) / increment);
		}

		ID3D12DescriptorHeap* GetHeapPtr();

	protected:
		virtual uint32_t GetDescriptorHandleIncrementSize() const;

		wil::com_ptr<DxDevice> m_device;
		wil::com_ptr<ID3D12DescriptorHeap> m_descriptor_heap;

		uint32_t m_num_descriptors = 0;
		uint32_t m_max_num_descriptors = 0;

		bool m_is_shader_visible = false;

		D3D12_DESCRIPTOR_HEAP_TYPE m_type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
	};

	class CbvSrvUavDescriptorHeap : public DescriptorHeap
	{
	public:
		CbvSrvUavDescriptorHeap(wil::com_ptr<DxDevice> d3d12device, uint32_t count, bool shader_visible = false) :
			DescriptorHeap(d3d12device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, count, shader_visible)
		{
		}
	};

	class SamplerDescriptorHeap : public DescriptorHeap
	{
	public:
		SamplerDescriptorHeap(wil::com_ptr<DxDevice> d3d12device, uint32_t count, bool shader_visible = false) :
			DescriptorHeap(d3d12device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, count, shader_visible)
		{
		}
	};

	class RtvDescriptorHeap : public DescriptorHeap
	{
	public:
		RtvDescriptorHeap(wil::com_ptr<DxDevice> d3d12device, uint32_t count, bool shader_visible = false) :
			DescriptorHeap(d3d12device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, count, shader_visible)
		{
		}
	};

	class DsvDescriptorHeap : public DescriptorHeap
	{
	public:
		DsvDescriptorHeap(wil::com_ptr<DxDevice> d3d12device, uint32_t count, bool shader_visible = false) :
			DescriptorHeap(d3d12device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, count, shader_visible)
		{
		}
	};
}

module :private;

namespace ysn
{
	DescriptorHeap::DescriptorHeap(wil::com_ptr<DxDevice> d3d12device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count, bool shader_visible)
	{
		m_type = type;
		m_max_num_descriptors = count;
		m_is_shader_visible = shader_visible;
		m_device = d3d12device;
	}

	bool DescriptorHeap::Initialize()
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = m_type;
		desc.NumDescriptors = m_max_num_descriptors;
		desc.Flags = m_is_shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		if (auto result = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptor_heap)); result != S_OK)
		{
			LogFatal << "Can't create descriptor heap: " << ConvertHrToString(result) << "\n";
			return false;
		}

		if constexpr (!IsReleaseActive())
		{
			switch (m_type)
			{
				case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
					m_descriptor_heap->SetName(L"CBV_SRV_UAV_DESCRIPTOR_HEAP");
					break;
				case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
					m_descriptor_heap->SetName(L"SAMPLER_DESCRIPTOR_HEAP");
					break;
				case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
					m_descriptor_heap->SetName(L"RTV_DESCRIPTOR_HEAP");
					break;
				case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
					m_descriptor_heap->SetName(L"DSV_DESCRIPTOR_HEAP");
					break;
				default:
					break;
			}
		}

		return true;
	}

	DescriptorHandle DescriptorHeap::GetNewHandle()
	{
		DescriptorHandle handle;
		handle.index = m_num_descriptors;

		m_num_descriptors += 1;

		if (m_num_descriptors >= m_max_num_descriptors)
		{
			// TODO: Error + error message!
		}

		handle.cpu = GetCpuHandle(handle);

		// TODO: Handle it more properly than this shit
		if (m_is_shader_visible)
		{
			handle.gpu = GetGpuHandle(handle);
		}

		return handle;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCpuHandle(DescriptorHandle handle)
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE start = m_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
		const uint32_t offset = GetDescriptorHandleIncrementSize();

		CD3DX12_CPU_DESCRIPTOR_HANDLE result(start, handle.index, offset);

		return result;
	}

	CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGpuHandle(DescriptorHandle handle)
	{
		const D3D12_GPU_DESCRIPTOR_HANDLE start = m_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
		const uint32_t offset = GetDescriptorHandleIncrementSize();

		CD3DX12_GPU_DESCRIPTOR_HANDLE result(start, handle.index, offset);

		return result;
	}

	ID3D12DescriptorHeap* DescriptorHeap::GetHeapPtr()
	{
		return m_descriptor_heap.get();
	}

	uint32_t DescriptorHeap::GetDescriptorHandleIncrementSize() const
	{
		return m_device->GetDescriptorHandleIncrementSize(m_type);
	}
}
