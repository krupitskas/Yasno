#pragma once

#include <memory>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wil/com.h>

#include <Renderer/CommandQueue.hpp>
#include <Renderer/DescriptorHeap.hpp>
#include <Renderer/ShaderStorage.hpp>
#include <Renderer/IndexStorage.hpp>
#include <Renderer/VertexStorage.hpp>
#include <Renderer/Pso.hpp>

namespace ysn
{
	class GenerateMipsSystem;

	class DxRenderer
	{
	public:
		bool Initialize();
		void Shutdown();

		bool CreateRootSignature(D3D12_ROOT_SIGNATURE_DESC* pRootSignatureDesc, ID3D12RootSignature** ppRootSignature) const;

		std::optional<PsoId> CreatePso(const GraphicsPsoDesc& pso_desc);
		std::optional<Pso> GetPso(PsoId pso_id);

		wil::com_ptr<ID3D12Device5> GetDevice() const;

		std::shared_ptr<CommandQueue> GetDirectQueue() const;
		std::shared_ptr<CommandQueue> GetComputeQueue() const;
		std::shared_ptr<CommandQueue> GetCopyQueue() const;

		std::shared_ptr<CbvSrvUavDescriptorHeap> GetCbvSrvUavDescriptorHeap() const;
		std::shared_ptr<SamplerDescriptorHeap> GetSamplerDescriptorHeap() const;
		std::shared_ptr<RtvDescriptorHeap> GetRtvDescriptorHeap() const;
		std::shared_ptr<DsvDescriptorHeap> GetDsvDescriptorHeap() const;

		std::shared_ptr<ShaderStorage> GetShaderStorage() const;
		std::shared_ptr<GenerateMipsSystem> GetMipGenerator() const;

		DXGI_FORMAT GetHdrFormat() const;
		DXGI_FORMAT GetBackBufferFormat() const;

		void FlushQueues();

		bool IsTearingSupported();

	private:
		bool CheckRaytracingSupport();

		// Constants
		static constexpr uint32_t SWAP_CHAIN_BUFFER_COUNT = 3;

		// Low level objects
		wil::com_ptr<ID3D12Device5> m_device;
		wil::com_ptr<IDXGIAdapter4> m_dxgi_adapter;
		wil::com_ptr<IDXGISwapChain4> m_swap_chain;

		// Data
		VertexStorage m_vertex_storage;
		IndexStorage m_index_storage;
		std::shared_ptr<ShaderStorage> m_shader_storage;
		PsoStorage m_pso_storage;

		// Formats
		DXGI_FORMAT hdr_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		DXGI_FORMAT backbuffer_format = DXGI_FORMAT_R8G8B8A8_UNORM;

		// Resources
		wil::com_ptr<ID3D12Resource> m_back_buffers[SWAP_CHAIN_BUFFER_COUNT];

		// Queues
		std::shared_ptr<CommandQueue> m_direct_command_queue;
		std::shared_ptr<CommandQueue> m_compute_command_queue;
		std::shared_ptr<CommandQueue> m_copy_command_queue;

		// Descriptor heaps
		std::shared_ptr<CbvSrvUavDescriptorHeap> m_cbv_srv_uav_descriptor_heap;
		std::shared_ptr<SamplerDescriptorHeap> m_sampler_descriptor_heap;
		std::shared_ptr<RtvDescriptorHeap> m_rtv_descriptor_heap;
		std::shared_ptr<DsvDescriptorHeap> m_dsv_descriptor_heap;

		// Info
		D3D_FEATURE_LEVEL m_d3d_feature_level = D3D_FEATURE_LEVEL_12_2;

		// Systems
		std::shared_ptr<GenerateMipsSystem> m_generate_mips_system;

		// Get back info
		bool m_tearing_supported = false;
		bool m_is_raytracing_supported = false;
	};
}
