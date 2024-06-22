#pragma once

#include <wil/com.h>
#include <d3d12.h>
#include <d3dx12.h>

#include <System/Result.hpp>

namespace ysn
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
		DescriptorHeap(wil::com_ptr<ID3D12Device5> d3d12device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count, bool shader_visible = false);

		DescriptorHandle GetNewHandle();
		CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(DescriptorHandle handle);
		CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(DescriptorHandle handle);

		ID3D12DescriptorHeap* GetHeapPtr();

	protected:
		virtual uint32_t GetDescriptorHandleIncrementSize() const;

		wil::com_ptr<ID3D12Device5> m_pd3d12device;
		wil::com_ptr<ID3D12DescriptorHeap> m_descriptor_heap;

		uint32_t m_num_descriptors = 0;
		uint32_t m_max_num_descriptors = 0;

		bool b_shader_visible = false;

		D3D12_DESCRIPTOR_HEAP_TYPE m_type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
	};

	class CbvSrvUavDescriptorHeap : public DescriptorHeap
	{
	public:
		CbvSrvUavDescriptorHeap(wil::com_ptr<ID3D12Device5> d3d12device, uint32_t count, bool shader_visible = false) :
			DescriptorHeap(d3d12device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, count, shader_visible)
		{}
	};

	class SamplerDescriptorHeap : public DescriptorHeap
	{
	public:
		SamplerDescriptorHeap(wil::com_ptr<ID3D12Device5> d3d12device, uint32_t count, bool shader_visible = false) :
			DescriptorHeap(d3d12device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, count, shader_visible)
		{}
	};

	class RtvDescriptorHeap : public DescriptorHeap
	{
	public:
		RtvDescriptorHeap(wil::com_ptr<ID3D12Device5> d3d12device, uint32_t count, bool shader_visible = false) :
			DescriptorHeap(d3d12device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, count, shader_visible)
		{}
	};

	class DsvDescriptorHeap : public DescriptorHeap
	{
	public:
		DsvDescriptorHeap(wil::com_ptr<ID3D12Device5> d3d12device, uint32_t count, bool shader_visible = false) :
			DescriptorHeap(d3d12device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, count, shader_visible)
		{}
	};

} // namespace ysn
