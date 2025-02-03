module;

#include <d3dx12.h>
#include <DirectXMath.h>

export module renderer.vertex_storage;

import std;

export namespace ysn
{
	using namespace DirectX;

	constexpr uint32_t g_max_debug_render_commands_count = 1024 * 1024;
	constexpr uint32_t g_max_debug_render_vertices_count = g_max_debug_render_commands_count * 2;

	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT2 uv0;

		Vertex& operator=(const Vertex& v)
		{
			position = v.position;
			normal = v.normal;
			//tangent = v.tangent;
			uv0 = v.uv0;
			return *this;
		}

		static std::vector<D3D12_INPUT_ELEMENT_DESC> GetVertexLayoutDesc();
	};

	struct DebugRenderVertex
	{
		XMFLOAT3 position;
		XMFLOAT3 color; // TODO: compress to uint

		static std::vector<D3D12_INPUT_ELEMENT_DESC> GetVertexLayoutDesc();
	};

	//struct VertexPosTexCoord
	//{
	//	XMFLOAT3 pos;
	//	XMFLOAT2 tex_coord;

	//	static std::vector<D3D12_INPUT_ELEMENT_DESC> GetVertexLayoutDesc();
	//};

	struct VertexStorage
	{
		// std::vector<Vertex> vertices;
	};

}

module :private;

namespace ysn
{
	std::vector<D3D12_INPUT_ELEMENT_DESC> Vertex::GetVertexLayoutDesc()
	{
		std::vector<D3D12_INPUT_ELEMENT_DESC> result;

		result.push_back(
			{ .SemanticName = "POSITION",
			 .Format = DXGI_FORMAT_R32G32B32_FLOAT,
			 .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
			 .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA });

		result.push_back(
			{ .SemanticName = "NORMAL",
			 .Format = DXGI_FORMAT_R32G32B32_FLOAT,
			 .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
			 .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA });

		//result.push_back(
		//	{ .SemanticName = "TANGENT",
		//	 .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
		//	 .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
		//	 .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA });

		result.push_back(
			{ .SemanticName = "TEXCOORD_",
			 .SemanticIndex = 0,
			 .Format = DXGI_FORMAT_R32G32_FLOAT,
			 .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
			 .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA });

		return result;
	}

	std::vector<D3D12_INPUT_ELEMENT_DESC> ysn::DebugRenderVertex::GetVertexLayoutDesc()
	{
		std::vector<D3D12_INPUT_ELEMENT_DESC> result;

		D3D12_INPUT_ELEMENT_DESC position = {};
		position.SemanticName = "POSITION";
		position.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		position.InputSlot = 0;
		position.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		position.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		D3D12_INPUT_ELEMENT_DESC color = {};
		color.SemanticName = "COLOR";
		color.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		color.InputSlot = 0;
		color.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		color.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		result.push_back(position);
		result.push_back(color);

		return result;
	}

	//std::vector<D3D12_INPUT_ELEMENT_DESC> ysn::VertexPosTexCoord::GetVertexLayoutDesc()
	//{
	//	std::vector<D3D12_INPUT_ELEMENT_DESC> elements_desc;

	//	D3D12_INPUT_ELEMENT_DESC position = {};
	//	position.SemanticName = "POSITION";
	//	position.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	//	position.InputSlot = 0;
	//	position.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	//	position.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

	//	D3D12_INPUT_ELEMENT_DESC texcoord = {};
	//	texcoord.SemanticName = "TEXCOORD";
	//	texcoord.Format = DXGI_FORMAT_R32G32_FLOAT;
	//	texcoord.InputSlot = 0;
	//	texcoord.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	//	texcoord.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

	//	elements_desc.push_back(position);
	//	elements_desc.push_back(texcoord);

	//	return elements_desc;
	//}
}
