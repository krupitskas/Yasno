#pragma once

#include <DirectXMath.h>
#include <d3d12.h>

#include <System/Game.hpp>
#include <System/Window.hpp>
#include <RHI/GpuTexture.hpp>
#include <Yasno/Camera.hpp>
#include <System/GltfLoader.hpp>
#include <Yasno/CameraController.hpp>
#include <System/GameInput.hpp>
#include <Graphics/Techniques/CascadedShadowMaps.hpp>
#include <Graphics/Techniques/TonemapPostprocess.hpp>
#include <Graphics/Techniques/RaytracingPass.hpp>
#include <Graphics/Techniques/ConvertToCubemap.hpp>
#include <Graphics/Techniques/SkyboxPass.hpp>
#include <RHI/RayTracing.hpp>
#include <Yasno/Lights.hpp>

namespace ysn
{
	SHADER_STRUCT GpuSceneParameters
	{
		DirectX::XMFLOAT4X4 shadow_matrix;
		DirectX::XMFLOAT4	directional_light_color		= {0.0f, 0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT4	directional_light_direction = {0.0f, 0.0f, 0.0f, 0.0f};
		float				directional_light_intensity = 0.0f;
		float				ambient_light_intensity		= 0.0f;
		uint32_t			shadows_enabled				= 0;

		uint32_t pad[1];

		static uint32_t GetGpuSize();
	};

	class Yasno : public Game
	{
	public:
		Yasno(const std::wstring& name, int width, int height, bool vSync = false);

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
		wil::com_ptr<ID3D12Resource> m_scene_color_buffer;
		wil::com_ptr<ID3D12Resource> m_back_buffer;

		DescriptorHandle m_hdr_uav_descriptor_handle;
		DescriptorHandle m_backbuffer_uav_descriptor_handle;
		DescriptorHandle m_depth_dsv_descriptor_handle;
		DescriptorHandle m_hdr_rtv_descriptor_handle;

		ModelRenderContext m_gltf_draw_context;
		RaytracingContext m_raytracing_context;
		DirectionalLight m_directional_light;
		EnvironmentLight m_environment_light;

		Texture m_environment_texture;
		wil::com_ptr<ID3D12Resource> m_cubemap_texture;

		D3D12_VIEWPORT m_viewport;
		D3D12_RECT m_scissors_rect;

		std::shared_ptr<Camera> m_camera;
		FpsCameraController m_camera_controler;
		wil::com_ptr<ID3D12Resource> m_camera_gpu_buffer;
		wil::com_ptr<ID3D12Resource> m_scene_parameters_gpu_buffer;

		bool m_is_raster = true;
		bool m_is_raster_pressed = false;

		bool m_is_content_loaded = false;
		bool m_is_first_frame = true;

		// Techniques
		CascadedShadowMaps m_shadow_pass;
		TonemapPostprocess m_tonemap_pass;
		RaytracingPass m_ray_tracing_pass;
		ConvertToCubemap m_convert_to_cubemap_pass;
		SkyboxPass m_skybox_pass;
	};
}
