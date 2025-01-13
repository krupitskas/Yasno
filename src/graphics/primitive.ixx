module;

#include <d3dx12.h>
#include <wil/com.h>

export module graphics.primitive;

import std;
import graphics.aabb;
import renderer.vertex_storage;
import renderer.dxrenderer;
import renderer.pso;
import system.application;
import system.logger;

export namespace ysn
{
	struct Primitive
	{
		uint32_t index = 0;

		AABB bbox;

		PsoId pso_id = -1;
		PsoId shadow_pso_id = -1;

		D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		D3D12_INDEX_BUFFER_VIEW index_buffer_view;
		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;

		uint32_t index_count = 0;
		uint32_t vertex_count = 0;

		int material_id = -1;

		// Moving to new way of render
		bool opaque = true; // for rtx
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
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

	std::optional<MeshPrimitive> ConstructBox();
	Primitive ConstructSphere(float radius, int latitude_segments, int longitude_segments);
}

module :private;

namespace ysn
{
	std::optional<MeshPrimitive> ConstructBox()
	{
		std::vector<Vertex> vertices;
		std::vector<uint16_t> indices;

		vertices = {
			// Front face
			{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}}, // Bottom-left
			{{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}}, // Top-left
			{{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}}, // Top-right
			{{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}}, // Bottom-right

			// Back face
			{{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f,  1.0f}, {0.0f, 1.0f}}, // Bottom-left
			{{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f,  1.0f}, {0.0f, 0.0f}}, // Top-left
			{{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f,  1.0f}, {1.0f, 0.0f}}, // Top-right
			{{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f,  1.0f}, {1.0f, 1.0f}}, // Bottom-right

			// Left face
			{{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // Bottom-left
			{{-1.0f,  1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // Top-left
			{{-1.0f,  1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // Top-right
			{{-1.0f, -1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // Bottom-right

			// Right face
			{{ 1.0f, -1.0f, -1.0f}, { 1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // Bottom-left
			{{ 1.0f,  1.0f, -1.0f}, { 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // Top-left
			{{ 1.0f,  1.0f,  1.0f}, { 1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // Top-right
			{{ 1.0f, -1.0f,  1.0f}, { 1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // Bottom-right

			// Top face
			{{-1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}, // Bottom-left
			{{-1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}, // Top-left
			{{ 1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // Top-right
			{{ 1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}, // Bottom-right

			// Bottom face
			{{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}}, // Bottom-left
			{{-1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}, // Top-left
			{{ 1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}}, // Top-right
			{{ 1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}}  // Bottom-right
		};

		// Define indices for each face (two triangles per face)
		indices = {
			0,  1,  2,  0,  2,  3,  // Front face
			4,  6,  5,  4,  7,  6,  // Back face
			8,  9,  10, 8,  10, 11, // Left face
			12, 14, 13, 12, 15, 14, // Right face
			16, 17, 18, 16, 18, 19, // Top face
			20, 22, 21, 20, 23, 22  // Bottom face
		};

		const uint32_t vertex_buffer_size = static_cast<uint32_t>(vertices.size()) * sizeof(Vertex);
		const uint32_t index_buffer_size = static_cast<uint32_t>(indices.size()) * sizeof(uint16_t);

		MeshPrimitive new_mesh;
		new_mesh.index_count = static_cast<uint32_t>(indices.size());
		new_mesh.vertex_count = static_cast<uint32_t>(vertices.size());
		new_mesh.input_layout_desc = Vertex::GetVertexLayoutDesc();

		const auto renderer = Application::Get().GetRenderer();

		// Note: using upload heaps to transfer static data like vert buffers is not
		// recommended. Every time the GPU needs it, the upload heap will be marshalled
		// over. Please read up on Default Heap usage. An upload heap is used here for
		// code simplicity and because there are very few verts to actually transfer.

		const auto heap_properties_upload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

		const auto vertex_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_size);

		HRESULT result = renderer->GetDevice()->CreateCommittedResource(
			&heap_properties_upload,
			D3D12_HEAP_FLAG_NONE,
			&vertex_buffer_desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&new_mesh.vertex_buffer));

		if (result != S_OK)
		{
			LogError << "Can't create vertex upload buffer\n";
			return std::nullopt;
		}

		const auto index_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(index_buffer_size);

		result = renderer->GetDevice()->CreateCommittedResource(
			&heap_properties_upload,
			D3D12_HEAP_FLAG_NONE,
			&index_buffer_desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&new_mesh.index_buffer));

		if (result != S_OK)
		{
			LogError << "Can't create index upload buffer\n";
			return std::nullopt;
		}

		{
			// Copy the triangle data to the vertex buffer.
			UINT8* vertex_data_begin;
			CD3DX12_RANGE read_range(0, 0); // We do not intend to read from this resource on the CPU.
			result = new_mesh.vertex_buffer->Map(0, &read_range, reinterpret_cast<void**>(&vertex_data_begin));

			if (result != S_OK)
			{
				LogError << "Can't map vertex buffer\n";
				return std::nullopt;
			}

			memcpy(vertex_data_begin, &vertices[0], vertex_buffer_size);
			new_mesh.vertex_buffer->Unmap(0, nullptr);
		}

		new_mesh.vertex_buffer_view.BufferLocation = new_mesh.vertex_buffer->GetGPUVirtualAddress();
		new_mesh.vertex_buffer_view.StrideInBytes = sizeof(Vertex);
		new_mesh.vertex_buffer_view.SizeInBytes = vertex_buffer_size;

		{
			// Copy the triangle data to the index buffer.
			UINT8* index_data_begin;
			CD3DX12_RANGE read_range(0, 0); // We do not intend to read from this resource on the CPU.
			result = new_mesh.index_buffer->Map(0, &read_range, reinterpret_cast<void**>(&index_data_begin));

			if (result != S_OK)
			{
				LogError << "Can't map index buffer\n";
				return std::nullopt;
			}

			memcpy(index_data_begin, &indices[0], index_buffer_size);
			new_mesh.index_buffer->Unmap(0, nullptr);
		}

		new_mesh.index_buffer_view.BufferLocation = new_mesh.index_buffer->GetGPUVirtualAddress();
		new_mesh.index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
		new_mesh.index_buffer_view.SizeInBytes = index_buffer_size;

		return new_mesh;
	}

	Primitive ConstructSphere(float radius, int latitude_segments, int longitude_segments)
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		const float PI = 3.14159265359f;

		// Generate vertices with normals
		for (int lat = 0; lat <= latitude_segments; ++lat) {
			float theta = lat * PI / latitude_segments; // Latitude angle (0 to PI)
			float sinTheta = std::sin(theta);
			float cosTheta = std::cos(theta);

			for (int lon = 0; lon <= longitude_segments; ++lon) {
				float phi = lon * 2 * PI / longitude_segments; // Longitude angle (0 to 2*PI)
				float sinPhi = std::sin(phi);
				float cosPhi = std::cos(phi);

				// Cartesian coordinates
				float x = sinTheta * cosPhi;
				float y = cosTheta;
				float z = sinTheta * sinPhi;

				// Scale position by radius
				float px = radius * x;
				float py = radius * y;
				float pz = radius * z;

				// Texture coordinates
				float u = static_cast<float>(lon) / longitude_segments;
				float v = static_cast<float>(lat) / latitude_segments;

				// Push vertex
				vertices.push_back({{px, py, pz}, {x, y, z}, {u, v}});
			}
		}

		// Generate indices
		for (int lat = 0; lat < latitude_segments; ++lat) {
			for (int lon = 0; lon < longitude_segments; ++lon) {
				int first = lat * (longitude_segments + 1) + lon;
				int second = first + longitude_segments + 1;

				// Two triangles per quad
				indices.push_back(first);
				indices.push_back(second);
				indices.push_back(first + 1);

				indices.push_back(second);
				indices.push_back(second + 1);
				indices.push_back(first + 1);
			}
		}

		const uint32_t vertex_buffer_size = static_cast<uint32_t>(vertices.size()) * sizeof(Vertex);
		const uint32_t index_buffer_size = static_cast<uint32_t>(indices.size()) * sizeof(uint16_t);

		/*

		const auto renderer = Application::Get().GetRenderer();

		// Note: using upload heaps to transfer static data like vert buffers is not
		// recommended. Every time the GPU needs it, the upload heap will be marshalled
		// over. Please read up on Default Heap usage. An upload heap is used here for
		// code simplicity and because there are very few verts to actually transfer.

		const auto heap_properties_upload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

		const auto vertex_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_size);

		HRESULT result = renderer->GetDevice()->CreateCommittedResource(
			&heap_properties_upload,
			D3D12_HEAP_FLAG_NONE,
			&vertex_buffer_desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&new_mesh.vertex_buffer));

		if (result != S_OK)
		{
			LogError << "Can't create vertex upload buffer\n";
			return std::nullopt;
		}

		const auto index_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(index_buffer_size);

		result = renderer->GetDevice()->CreateCommittedResource(
			&heap_properties_upload,
			D3D12_HEAP_FLAG_NONE,
			&index_buffer_desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&new_mesh.index_buffer));

		if (result != S_OK)
		{
			LogError << "Can't create index upload buffer\n";
			return std::nullopt;
		}

		{
			// Copy the triangle data to the vertex buffer.
			UINT8* vertex_data_begin;
			CD3DX12_RANGE read_range(0, 0); // We do not intend to read from this resource on the CPU.
			result = new_mesh.vertex_buffer->Map(0, &read_range, reinterpret_cast<void**>(&vertex_data_begin));

			if (result != S_OK)
			{
				LogError << "Can't map vertex buffer\n";
				return std::nullopt;
			}

			memcpy(vertex_data_begin, &vertices[0], vertex_buffer_size);
			new_mesh.vertex_buffer->Unmap(0, nullptr);
		}

		new_mesh.vertex_buffer_view.BufferLocation = new_mesh.vertex_buffer->GetGPUVirtualAddress();
		new_mesh.vertex_buffer_view.StrideInBytes = sizeof(Vertex);
		new_mesh.vertex_buffer_view.SizeInBytes = vertex_buffer_size;

		{
			// Copy the triangle data to the index buffer.
			UINT8* index_data_begin;
			CD3DX12_RANGE read_range(0, 0); // We do not intend to read from this resource on the CPU.
			result = new_mesh.index_buffer->Map(0, &read_range, reinterpret_cast<void**>(&index_data_begin));

			if (result != S_OK)
			{
				LogError << "Can't map index buffer\n";
				return std::nullopt;
			}

			memcpy(index_data_begin, &indices[0], index_buffer_size);
			new_mesh.index_buffer->Unmap(0, nullptr);
		}
		*/

		Primitive new_primitive;
		new_primitive.vertices = vertices;
		new_primitive.indices = indices;
		new_primitive.index_count = indices.size();
		new_primitive.vertex_count = vertices.size();

		return new_primitive;
	}
}
