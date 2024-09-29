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
		wil::com_ptr<ID3D12Resource> vertex_buffer;
		uint64_t vertex_offset_in_bytes;
		uint32_t vertex_count;
		uint32_t vertex_stride;

		wil::com_ptr<ID3D12Resource> index_buffer;
		uint64_t index_offset_in_bytes;
		uint32_t index_count;
	};

	struct RaytracingContext
	{
		/// Create the acceleration structure of an instance
		/// vertex_buffers : pair of buffer and vertex count
		AccelerationStructureBuffers CreateBottomLevelAS(
			wil::com_ptr<ID3D12Device5> device,
			wil::com_ptr<ID3D12GraphicsCommandList4> command_list,
			std::vector<BLASVertexInput> vertex_buffers); // sizeof(Vertex)

		/// Create the main acceleration structure that holds
		/// all instances of the scene
		/// \param     instances : pair of BLAS and transform
		void CreateTopLevelAS(
			wil::com_ptr<ID3D12Device5> device,
			wil::com_ptr<ID3D12GraphicsCommandList4> command_list,
			const std::vector<std::pair<wil::com_ptr<ID3D12Resource>, DirectX::XMMATRIX>>& instances);

		void CreateTlasSrv(std::shared_ptr<ysn::DxRenderer> renderer);

		/// Create all acceleration structures, bottom and top
		void CreateAccelerationStructures(
			wil::com_ptr<ID3D12Device5> device,
			wil::com_ptr<ID3D12GraphicsCommandList4> command_list,
			ysn::ModelRenderContext* gltf_model_context);

		nv_helpers_dx12::TopLevelASGenerator tlas_generator;

		wil::com_ptr<ID3D12Resource> blas_res;

		AccelerationStructureBuffers tlas_buffers;
		std::vector<std::pair<wil::com_ptr<ID3D12Resource>, DirectX::XMMATRIX>> instances;
	};

} 