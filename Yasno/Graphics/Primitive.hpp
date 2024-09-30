#pragma once

#include <unordered_map>

#include <d3dx12.h>
#include <wil/com.h>

#include <Renderer/VertexStorage.hpp>

namespace ysn
{
	struct Attribute
	{
		std::string name;

		uint32_t vertex_count = 0;

		DXGI_FORMAT format;
		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
	};

	struct Primitive
	{
		D3D_PRIMITIVE_TOPOLOGY topology;
		D3D12_INDEX_BUFFER_VIEW index_buffer_view;

		uint32_t index_count = 0;

		std::unordered_map<std::string, Attribute> attributes;
	};

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
