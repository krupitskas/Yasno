#pragma once

import graphics.aabb;
import renderer.vertex_storage;

import <unordered_map>;
import <vector>;

#include <d3dx12.h>
#include <wil/com.h>

#include <Renderer/Pso.hpp>

namespace ysn
{
	struct Primitive
	{
		uint32_t index = 0;

		AABB bbox;

		PsoId pso_id = -1;
		PsoId shadow_pso_id = -1;

		D3D_PRIMITIVE_TOPOLOGY topology;

		D3D12_INDEX_BUFFER_VIEW index_buffer_view;
		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;

		uint32_t index_count = 0;
		uint32_t vertex_count = 0;

		int material_id = -1;

		// Moving to new way of render
		bool					opaque = true; // for rtx
		std::vector<Vertex>		vertices;
		std::vector<uint32_t>	indices;
		uint32_t global_vertex_offset = 0;
		uint32_t global_index_offset = 0;
	};

	// TODO: Rename it into some CSG mesh
	struct MeshPrimitive
	{
		wil::com_ptr<ID3D12Resource> vertex_buffer;
		wil::com_ptr<ID3D12Resource> index_buffer;
		
		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
		D3D12_INDEX_BUFFER_VIEW index_buffer_view;

		uint32_t index_count = 0;
		uint32_t vertex_count = 0;

		std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout_desc;
	};

	MeshPrimitive ConstructBox();
}
