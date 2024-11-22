#pragma once

#include <vector>
#include <memory>

#include <wil/com.h>
#include <d3d12.h>

#include <Renderer/nv_helpers_dx12/TopLevelASGenerator.h>
#include <Renderer/DxRenderer.hpp>

#include <Graphics/RenderScene.hpp>

namespace ysn
{
	struct AccelerationStructureBuffers
	{
		wil::com_ptr<ID3D12Resource> scratch; // Scratch memory for AS builder
		wil::com_ptr<ID3D12Resource> result; // Where the AS is
		wil::com_ptr<ID3D12Resource> instance_desc; // Hold the matrices of the instances

		DescriptorHandle tlas_srv;
	};

	struct BLASVertexInput
	{
		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
		D3D12_INDEX_BUFFER_VIEW index_buffer_view;

		uint32_t vertex_count;
		uint32_t index_count;
	};

	struct TlasInput
	{
		wil::com_ptr<ID3D12Resource> blas;
		DirectX::XMMATRIX transform;
		uint32_t instance_id = 0;
	};

	struct RaytracingContext
	{
		AccelerationStructureBuffers CreateBottomLevelAS(
			wil::com_ptr<ID3D12Device5> device,
			wil::com_ptr<ID3D12GraphicsCommandList4> command_list,
			std::vector<BLASVertexInput> vertex_buffers);

		void CreateTopLevelAS(
			wil::com_ptr<ID3D12Device5> device,
			wil::com_ptr<ID3D12GraphicsCommandList4> command_list,
			const std::vector<TlasInput>& instances);

		void CreateTlasSrv(std::shared_ptr<ysn::DxRenderer> renderer);

		void CreateAccelerationStructures(wil::com_ptr<ID3D12GraphicsCommandList4> command_list, const RenderScene& render_scene);

		nv_helpers_dx12::TopLevelASGenerator tlas_generator;

		std::vector<wil::com_ptr<ID3D12Resource>> blas_res;

		AccelerationStructureBuffers tlas_buffers;
		std::vector<TlasInput> instances;
	};

} 