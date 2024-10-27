#pragma once

#include <Graphics/RenderScene.hpp>
#include <Graphics/Primitive.hpp>
#include <Renderer/DescriptorHeap.hpp>
#include <Renderer/CommandQueue.hpp>
#include <Renderer/GpuTexture.hpp>
#include <Graphics/Techniques/ShadowMapPass.hpp>

namespace ysn
{

	struct ForwardPassRenderParameters
	{
		std::shared_ptr<CommandQueue> command_queue = nullptr;
		std::shared_ptr<CbvSrvUavDescriptorHeap> cbv_srv_uav_heap = nullptr;
		wil::com_ptr<ID3D12Resource> scene_color_buffer = nullptr;
		DescriptorHandle hdr_rtv_descriptor_handle;
		DescriptorHandle dsv_descriptor_handle;
		D3D12_VIEWPORT viewport;
		D3D12_RECT scissors_rect;
		wil::com_ptr<ID3D12Resource> camera_gpu_buffer;
		wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer;
		D3D12_CPU_DESCRIPTOR_HANDLE backbuffer_handle;
		wil::com_ptr<ID3D12Resource> current_back_buffer;
		ShadowMapBuffer shadow_map_buffer;
	};

	struct ForwardPass
	{
		//void Initialize();
		bool CompilePrimitivePso(ysn::Primitive& primitive, std::vector<Material> materials);
		void Render(const RenderScene& render_scene, const ForwardPassRenderParameters& render_parameters);
		void RenderIndirect(const RenderScene& render_scene, const ForwardPassRenderParameters& render_parameters);

		bool m_enable_culling = false;

		// TODO(indirect):
		// 1. Big buffer of materials
		// 2. Big buffer of transformations (as for instancing)

	};
}
