#pragma once

#include <unordered_map>

#include <d3dx12.h>
#include <wil/com.h>

#include <Renderer/Pso.hpp>
#include <Renderer/VertexStorage.hpp>
#include <Graphics/AABB.hpp>

namespace ysn
{
	struct Attribute
	{
		std::string name;

		DXGI_FORMAT format;
		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
	};

	struct Primitive
	{
		PsoId pso_id = -1;
		PsoId shadow_pso_id = -1;

		D3D_PRIMITIVE_TOPOLOGY topology;

		D3D12_INDEX_BUFFER_VIEW index_buffer_view;
		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;

		uint32_t index_count = 0;
		uint32_t vertex_count = 0;

		int material_id = -1;

		std::unordered_map<std::string, Attribute> attributes;

		// Moving to new way of render
		bool					opaque = true; // for rtx
		AABB					bbox;
		std::vector<Vertex>		vertices;
		std::vector<uint32_t>	indices;
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
