#pragma once

#include <cstdint>

#include <SimpleMath.h>
#include <DirectXMath.h>
#include <d3dx12.h>
#include <wil/com.h>

#include <System/GltfLoader.hpp>
#include <Renderer/DescriptorHeap.hpp>
#include <Renderer/DxRenderer.hpp>

namespace ysn
{
	using namespace DirectX;

	struct DirectionalLight;
	class CommandQueue;

	struct ShadowMapBuffer
	{
		wil::com_ptr<ID3D12Resource> buffer;
		DescriptorHandle dsv_handle;
		DescriptorHandle srv_handle;
	};

	struct ShadowRenderParameters
	{
		std::shared_ptr<ysn::CommandQueue> command_queue;
		wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer;
	};

	struct ShadowMapPass
	{
		void Initialize(std::shared_ptr<ysn::DxRenderer> p_renderer);
		bool CompilePrimitivePso(ysn::Primitive& primitive, std::vector<Material> materials);
		void UpdateLight(const DirectionalLight& Light);
		void Render(const RenderScene& render_scene, const ShadowRenderParameters& parameters);

		// Shadow Map resources
		std::uint32_t ShadowMapDimension = 4096;

		XMMATRIX shadow_matrix;

		D3D12_VIEWPORT Viewport;
		D3D12_RECT ScissorRect;

		ShadowMapBuffer shadow_map_buffer;

	private:
		bool InitializeShadowMapBuffer(std::shared_ptr<ysn::DxRenderer> p_renderer);
		void InitializeOrthProjection(SimpleMath::Vector3 direction);
		bool InitializeCamera(std::shared_ptr<ysn::DxRenderer> p_renderer);

		wil::com_ptr<ID3D12Resource> m_camera_buffer;
	};
}
