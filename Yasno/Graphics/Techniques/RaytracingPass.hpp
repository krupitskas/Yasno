#pragma once

// https://developer.nvidia.com/rtx/raytracing/dxr/DX12-Raytracing-tutorial-Part-2

#include <memory>

#include <wil/com.h>
#include <d3d12.h>
#include <dxcapi.h>

#include <RHI/nv_helpers_dx12/ShaderBindingTableGenerator.h>

namespace ysn
{
	class D3D12Renderer;
	struct RaytracingContext;
	struct DescriptorHandle;

	class RaytracingPass
	{
	public:
		bool Initialize(std::shared_ptr<ysn::D3D12Renderer> renderer, wil::com_ptr<ID3D12Resource> scene_color, RaytracingContext& rtx_context, wil::com_ptr<ID3D12Resource> camera_buffer);
		bool CreateRaytracingPipeline(std::shared_ptr<ysn::D3D12Renderer> renderer);
		bool CreateShaderBindingTable(std::shared_ptr<ysn::D3D12Renderer> renderer, wil::com_ptr<ID3D12Resource> scene_color, RaytracingContext& rtx_context, wil::com_ptr<ID3D12Resource> camera_buffer);

		void Execute(std::shared_ptr<ysn::D3D12Renderer> renderer, wil::com_ptr<ID3D12GraphicsCommandList4> command_list, uint32_t width, uint32_t height, wil::com_ptr<ID3D12Resource> scene_color, wil::com_ptr<ID3D12Resource> camera_buffer);

		wil::com_ptr<ID3D12RootSignature> CreateRayGenSignature(std::shared_ptr<ysn::D3D12Renderer> renderer);
		wil::com_ptr<ID3D12RootSignature> CreateMissSignature(std::shared_ptr<ysn::D3D12Renderer> renderer);
		wil::com_ptr<ID3D12RootSignature> CreateHitSignature(std::shared_ptr<ysn::D3D12Renderer> renderer);

		wil::com_ptr<IDxcBlob> m_ray_gen_library;
		wil::com_ptr<IDxcBlob> m_hit_library;
		wil::com_ptr<IDxcBlob> m_miss_library;

		wil::com_ptr<ID3D12RootSignature> m_ray_gen_signature;
		wil::com_ptr<ID3D12RootSignature> m_hit_signature;
		wil::com_ptr<ID3D12RootSignature> m_miss_signature;

		wil::com_ptr<ID3D12Resource> m_sbt_storage;

		// Ray tracing pipeline state
		wil::com_ptr<ID3D12StateObject> m_rt_state_object;

		// Ray tracing pipeline state properties, retaining the shader identifiers
		// to use in the Shader Binding Table
		wil::com_ptr<ID3D12StateObjectProperties> m_rt_state_object_props;

		nv_helpers_dx12::ShaderBindingTableGenerator m_sbt_helper;
	};
}
