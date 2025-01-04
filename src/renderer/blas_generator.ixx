module;

#include <d3d12.h>

export module renderer.blas_generator;

import std;
import renderer.dx_types;
import renderer.gpu_buffer;
import system.math;
import system.logger;

export namespace ysn
{
	class BlasGenerator
	{
	public:
		void AddVertexBuffer(
			D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view,
			D3D12_INDEX_BUFFER_VIEW index_buffer_view,
			std::uint32_t vertex_count,
			std::uint32_t index_count,
			ID3D12Resource* transform_buffer,
			UINT64 transform_offset_in_bytes,
			bool is_opaque = true);

		void ComputeBlasBufferSizes(DxDevice* device, bool allow_update, UINT64* scratch_size_in_bytes, UINT64* result_size_in_bytes);

		bool Generate(
			DxGraphicsCommandList* cmd_list,
			const ysn::GpuBuffer& scratch_buffer,
			ysn::GpuBuffer& result_buffer,
			bool update_only = false,
			ID3D12Resource* previous_result = nullptr);

	private:
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_vertex_buffers = {};

		UINT64 m_scratch_size_in_bytes = 0;
		UINT64 m_result_size_in_bytes = 0;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS m_flags;
	};
}

module :private;

namespace ysn
{
	void BlasGenerator::AddVertexBuffer(
		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view,
		D3D12_INDEX_BUFFER_VIEW index_buffer_view,
		std::uint32_t vertex_count,
		std::uint32_t index_count,
		ID3D12Resource* transform_buffer,
		UINT64 transform_offset_in_bytes,
		bool is_opaque)
	{
		D3D12_RAYTRACING_GEOMETRY_DESC descriptor = {};
		descriptor.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		descriptor.Triangles.VertexBuffer.StartAddress = vertex_buffer_view.BufferLocation;
		descriptor.Triangles.VertexBuffer.StrideInBytes = vertex_buffer_view.StrideInBytes;
		descriptor.Triangles.VertexCount = vertex_count;
		descriptor.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		descriptor.Triangles.IndexBuffer = index_buffer_view.BufferLocation;
		descriptor.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		descriptor.Triangles.IndexCount = index_count;
		descriptor.Triangles.Transform3x4 = transform_buffer ? (transform_buffer->GetGPUVirtualAddress() + transform_offset_in_bytes) : 0;
		descriptor.Flags = is_opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

		m_vertex_buffers.push_back(descriptor);
	}

	void BlasGenerator::ComputeBlasBufferSizes(DxDevice* device, bool allow_update, UINT64* scratch_size_in_bytes, UINT64* result_size_in_bytes)
	{
		m_flags = allow_update ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
			: D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS build_desc;
		build_desc.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		build_desc.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		build_desc.NumDescs = static_cast<UINT>(m_vertex_buffers.size());
		build_desc.pGeometryDescs = m_vertex_buffers.data();
		build_desc.Flags = m_flags;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
		device->GetRaytracingAccelerationStructurePrebuildInfo(&build_desc, &info);

		*scratch_size_in_bytes = ysn::AlignPow2(info.ScratchDataSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		*result_size_in_bytes = ysn::AlignPow2(info.ResultDataMaxSizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		m_scratch_size_in_bytes = *scratch_size_in_bytes;
		m_result_size_in_bytes = *result_size_in_bytes;
	}

	bool BlasGenerator::Generate(
		DxGraphicsCommandList* cmd_list,
		const ysn::GpuBuffer& scratch_buffer,
		ysn::GpuBuffer& result_buffer,
		bool update_only,
		ID3D12Resource* previous_result)
	{
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = m_flags;

		if (flags == D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && update_only)
		{
			flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		}

		if (m_flags != D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE && update_only)
		{
			ysn::LogFatal << "Cannot update a bottom-level AS not originally built for updates\n";
			return false;
		}

		if (update_only && previous_result == nullptr)
		{
			ysn::LogFatal << "Bottom-level hierarchy update requires the previous hierarchy\n";
			return false;
		}

		if (m_result_size_in_bytes == 0 || m_scratch_size_in_bytes == 0)
		{
			ysn::LogFatal << "Invalid scratch and result buffer sizes - ComputeBlasBufferSizes needs to be called before Build\n";
			return false;
		}

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc;
		build_desc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		build_desc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		build_desc.Inputs.NumDescs = static_cast<UINT>(m_vertex_buffers.size());
		build_desc.Inputs.pGeometryDescs = m_vertex_buffers.data();
		build_desc.DestAccelerationStructureData = { result_buffer.GPUVirtualAddress() };
		build_desc.ScratchAccelerationStructureData = { scratch_buffer.GPUVirtualAddress() };
		build_desc.SourceAccelerationStructureData = previous_result ? previous_result->GetGPUVirtualAddress() : 0;
		build_desc.Inputs.Flags = flags;

		cmd_list->BuildRaytracingAccelerationStructure(&build_desc, 0, nullptr);

		D3D12_RESOURCE_BARRIER uav_barrier;
		uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uav_barrier.UAV.pResource = result_buffer.Resource();
		uav_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		cmd_list->ResourceBarrier(1, &uav_barrier);

		return true;
	}
}
