#pragma once

#include <DirectXMath.h>
#include <d3d12.h>

#include <System/Game.hpp>
#include <System/Window.hpp>
#include <System/GameInput.hpp>
#include <System/GltfLoader.hpp>
#include <Renderer/RaytracingContext.hpp>
#include <Renderer/GpuTexture.hpp>
#include <Graphics/Techniques/ShadowMapPass.hpp>
#include <Graphics/Techniques/TonemapPostprocess.hpp>
#include <Graphics/Techniques/RaytracingPass.hpp>
#include <Graphics/Techniques/ConvertToCubemap.hpp>
#include <Graphics/Techniques/SkyboxPass.hpp>
#include <Graphics/Techniques/ForwardPass.hpp>

import yasno.camera_controller;

namespace ysn
{
	class Yasno : public Game
	{
	public:
		Yasno(const std::wstring& name, int width, int height, bool vsync = false);

		bool LoadContent() override;
		void UnloadContent() override;
		void Destroy() override;

	protected:
		void OnUpdate(UpdateEventArgs& e) override;

		void OnRender(RenderEventArgs& e) override;

		void RenderUi();

		void OnResize(ResizeEventArgs& e) override;

	private:
		GameInput game_input;
		RenderScene m_render_scene;

		bool CreateGpuCameraBuffer();
		bool CreateGpuSceneParametersBuffer();

		void UpdateGpuCameraBuffer();
		void UpdateGpuSceneParametersBuffer();

		void UpdateBufferResource(wil::com_ptr<ID3D12GraphicsCommandList2> commandList,
								  ID3D12Resource** pDestinationResource,
								  ID3D12Resource** pIntermediateResource,
								  size_t numElements,
								  size_t elementSize,
								  const void* bufferData,
								  D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

		void ResizeDepthBuffer(int width, int height);
		void ResizeHdrBuffer(int width, int height);
		void ResizeBackBuffer(int width, int height);

		uint64_t m_fence_values[Window::BufferCount] = {};

		wil::com_ptr<ID3D12Resource> m_depth_buffer;
		wil::com_ptr<ID3D12Resource> m_back_buffer;
		// Two HDR buffers for history
		wil::com_ptr<ID3D12Resource> m_scene_color_buffer;
		wil::com_ptr<ID3D12Resource> m_scene_color_buffer_1;

		DescriptorHandle m_hdr_uav_descriptor_handle;
		DescriptorHandle m_backbuffer_uav_descriptor_handle;
		DescriptorHandle m_depth_dsv_descriptor_handle;
		DescriptorHandle m_hdr_rtv_descriptor_handle;

		RaytracingContext m_raytracing_context;

		D3D12_VIEWPORT m_viewport;
		D3D12_RECT m_scissors_rect;

		GpuTexture m_environment_texture;
		wil::com_ptr<ID3D12Resource> m_cubemap_texture;

		wil::com_ptr<ID3D12Resource> m_camera_gpu_buffer;
		wil::com_ptr<ID3D12Resource> m_scene_parameters_gpu_buffer;

		bool m_is_raster = true;
		bool m_is_indirect = false;
		bool m_is_raster_pressed = false;

		bool m_is_content_loaded = false;
		bool m_is_first_frame = true;
		bool m_reset_rtx_accumulation = true;
		bool m_is_rtx_accumulation_enabled = false;

		// Techniques
		ForwardPass m_forward_pass;
		ShadowMapPass m_shadow_pass;
		TonemapPostprocess m_tonemap_pass;
		RaytracingPass m_ray_tracing_pass;
		ConvertToCubemap m_convert_to_cubemap_pass;
		SkyboxPass m_skybox_pass;
	};
}
