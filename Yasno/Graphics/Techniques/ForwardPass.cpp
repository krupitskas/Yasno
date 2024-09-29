#include "ForwardPass.hpp"

namespace ysn
{
	/*
	void ForwardPass::Render(const RenderScene& scene)
	{

		wil::com_ptr<ID3D12GraphicsCommandList4> command_list = command_queue->GetCommandList();

		PIXBeginEvent(command_list.get(), PIX_COLOR_DEFAULT, "GeometryPass");

		// Clear the render targets.
		{
			FLOAT clear_color[] = { 44.0f / 255.0f, 58.f / 255.0f, 74.0f / 255.0f, 1.0f };

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(current_back_buffer.get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
			command_list->ResourceBarrier(1, &barrier);

			command_list->ClearRenderTargetView(backbuffer_handle, clear_color, 0, nullptr);
			command_list->ClearDepthStencilView(m_depth_dsv_descriptor_handle.cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			command_list->ClearRenderTargetView(m_hdr_rtv_descriptor_handle.cpu, clear_color, 0, nullptr);
		}

		command_list->RSSetViewports(1, &m_viewport);
		command_list->RSSetScissorRects(1, &m_scissors_rect);
		command_list->OMSetRenderTargets(1, &m_hdr_rtv_descriptor_handle.cpu, FALSE, &m_depth_dsv_descriptor_handle.cpu);

		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_shadow_pass.shadow_map_buffer.buffer.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			command_list->ResourceBarrier(1, &barrier);
		}

		//RenderGLTF(&m_gltf_draw_context,
		//		   Application::Get().GetRenderer(),
		//		   &m_gltf_draw_context.gltfModel,
		//		   command_list,
		//		   m_camera_gpu_buffer,
		//		   m_scene_parameters_gpu_buffer,
		//		   PrimitivePipeline::ForwardPbr,
		//		   &m_shadow_pass.shadow_map_buffer);


		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_shadow_pass.shadow_map_buffer.buffer.get(),
																					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
																					D3D12_RESOURCE_STATE_DEPTH_WRITE);
			command_list->ResourceBarrier(1, &barrier);
		}

		PIXEndEvent(command_list.get());

		command_queue->ExecuteCommandList(command_list);
	}
	*/

}
