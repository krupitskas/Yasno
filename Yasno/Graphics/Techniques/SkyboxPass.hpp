#pragma once

#include <Graphics/Primitives.hpp>
#include <RHI/DescriptorHeap.hpp>
#include <RHI/CommandQueue.hpp>
#include <RHI/GpuTexture.hpp>

namespace ysn
{
	struct SkyboxPassParameters
	{
		std::shared_ptr<CommandQueue> command_queue = nullptr;
		std::shared_ptr<CbvSrvUavDescriptorHeap> cbv_srv_uav_heap = nullptr;
		wil::com_ptr<ID3D12Resource> scene_color_buffer = nullptr;
		DescriptorHandle hdr_rtv_descriptor_handle;
		DescriptorHandle dsv_descriptor_handle;
		D3D12_VIEWPORT viewport;
		D3D12_RECT scissors_rect;
		Texture* equirectangular_texture;
		wil::com_ptr<ID3D12Resource> camera_gpu_buffer;
	};
	
	class SkyboxPass
	{
		enum ShaderInputParameters
		{
			ViewParameters,
			InputTexture,
			ParametersCount
		};
	public:
		bool Initialize();
		void RenderSkybox(SkyboxPassParameters* parameters);
	private:
		void UpdateParameters();
		
		MeshPrimitive cube;

		wil::com_ptr<ID3D12RootSignature> m_root_signature;
		wil::com_ptr<ID3D12PipelineState> m_pipeline_state;
	};
}
