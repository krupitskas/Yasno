#include "DescriptorHeap.hpp"

#include <System/Helpers.hpp>

namespace ysn
{
	DescriptorHeap::DescriptorHeap(wil::com_ptr<ID3D12Device5> d3d12device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count, bool shader_visible)
	{
		m_type = type;
		m_max_num_descriptors = count;
		b_shader_visible = shader_visible;

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = m_type;
		desc.NumDescriptors = m_max_num_descriptors;
		desc.Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		ThrowIfFailed(d3d12device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptor_heap)));

	#ifndef YSN_RELEASE
		switch (type)
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
	#endif

		m_pd3d12device = d3d12device;
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
		if (b_shader_visible)
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
		return m_pd3d12device->GetDescriptorHandleIncrementSize(m_type);
	}
} // namespace ysn
