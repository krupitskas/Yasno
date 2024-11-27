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

	// Data structure to match the command signature used for ExecuteIndirect.
	struct IndirectCommand
	{
		D3D12_GPU_VIRTUAL_ADDRESS camera_parameters_cbv;
		D3D12_GPU_VIRTUAL_ADDRESS scene_parameters_cbv;
		D3D12_GPU_VIRTUAL_ADDRESS per_instance_data_cbv;
		D3D12_DRAW_INDEXED_ARGUMENTS draw_arguments;
	};

	enum class IndirectRootParameters : uint8_t
	{
		CameraParametersSrv,
		SceneParametersSrv,
		PerInstanceDataSrv,
		Count
	};

	struct ForwardPass
	{
		bool Initialize(const RenderScene& render_scene, wil::com_ptr<ID3D12Resource> camera_gpu_buffer, wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer, const GpuBuffer& instance_buffer, wil::com_ptr<ID3D12GraphicsCommandList4> cmd_list);
		bool InitializeIndirectPipeline(const RenderScene& render_scene, wil::com_ptr<ID3D12Resource> camera_gpu_buffer, wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer, const GpuBuffer& instance_buffer, wil::com_ptr<ID3D12GraphicsCommandList4> cmd_list);
		bool CompilePrimitivePso(ysn::Primitive& primitive, std::vector<Material> materials);
		void Render(const RenderScene& render_scene, const ForwardPassRenderParameters& render_parameters);
		void RenderIndirect(const RenderScene& render_scene, const ForwardPassRenderParameters& render_parameters);

		// Indirect data
		wil::com_ptr<ID3D12RootSignature> m_indirect_root_signature;
		std::vector<IndirectCommand> m_indirect_commands;
		wil::com_ptr<ID3D12CommandSignature> m_command_signature;
		GpuBuffer m_command_buffer;
		PsoId indirect_pso_id = 0;
		uint32_t m_command_buffer_size = 0; // PrimitiveCount * sizeof(IndirectCommand)
	};
}
