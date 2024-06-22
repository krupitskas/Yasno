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

export namespace ysn
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

MeshPrimitive ConstructBox();
} // namespace ysn

module :private;

namespace ysn
{
MeshPrimitive ConstructBox()
{
    std::vector<VertexPosTexCoord> vertices = {
        // Front face
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}}, // 0: Bottom-left
        {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}},  // 1: Top-left
        {{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}},   // 2: Top-right
        {{1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}},  // 3: Bottom-right

        // Back face
        {{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}}, // 4: Bottom-left
        {{-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},  // 5: Top-left
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},   // 6: Top-right
        {{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}}   // 7: Bottom-right
    };

    std::vector<uint16_t> indices = {// Front face
                                     0,
                                     1,
                                     2,
                                     0,
                                     2,
                                     3,

                                     // Back face
                                     4,
                                     6,
                                     5,
                                     4,
                                     7,
                                     6,

                                     // Left face
                                     4,
                                     5,
                                     1,
                                     4,
                                     1,
                                     0,

                                     // Right face
                                     3,
                                     2,
                                     6,
                                     3,
                                     6,
                                     7,

                                     // Top face
                                     1,
                                     5,
                                     6,
                                     1,
                                     6,
                                     2,

                                     // Bottom face
                                     4,
                                     0,
                                     3,
                                     4,
                                     3,
                                     7};

    const uint32_t vertex_buffer_size = static_cast<uint32_t>(vertices.size()) * sizeof(VertexPosTexCoord);
    const uint32_t index_buffer_size = static_cast<uint32_t>(indices.size()) * sizeof(uint16_t);

    MeshPrimitive new_box;
    new_box.index_count = static_cast<uint32_t>(indices.size());
    new_box.vertex_count = static_cast<uint32_t>(vertices.size());
    new_box.input_layout_desc = VertexPosTexCoord::GetVertexLayoutDesc();

    const auto renderer = Application::Get().GetRenderer();

    // Note: using upload heaps to transfer static data like vert buffers is not
    // recommended. Every time the GPU needs it, the upload heap will be marshalled
    // over. Please read up on Default Heap usage. An upload heap is used here for
    // code simplicity and because there are very few verts to actually transfer.

    const auto heap_properties_upload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    const auto vertex_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(vertex_buffer_size);

    // TODO: ThrowIfFailed
    renderer->GetDevice()->CreateCommittedResource(
        &heap_properties_upload,
        D3D12_HEAP_FLAG_NONE,
        &vertex_buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&new_box.vertex_buffer));

    const auto index_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(index_buffer_size);

    // TODO: ThrowIfFailed
    renderer->GetDevice()->CreateCommittedResource(
        &heap_properties_upload,
        D3D12_HEAP_FLAG_NONE,
        &index_buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&new_box.index_buffer));

    {
        // Copy the triangle data to the vertex buffer.
        UINT8* vertex_data_begin;
        CD3DX12_RANGE read_range(0, 0); // We do not intend to read from this resource on the CPU.
        new_box.vertex_buffer->Map(0, &read_range, reinterpret_cast<void**>(&vertex_data_begin)); // TODO: ThrowIfFailed
        memcpy(vertex_data_begin, &vertices[0], vertex_buffer_size);
        new_box.vertex_buffer->Unmap(0, nullptr);
    }

    new_box.vertex_buffer_view.BufferLocation = new_box.vertex_buffer->GetGPUVirtualAddress();
    new_box.vertex_buffer_view.StrideInBytes = sizeof(VertexPosTexCoord);
    new_box.vertex_buffer_view.SizeInBytes = vertex_buffer_size;

    {
        // Copy the triangle data to the index buffer.
        UINT8* index_data_begin;
        CD3DX12_RANGE read_range(0, 0); // We do not intend to read from this resource on the CPU.
        new_box.index_buffer->Map(0, &read_range, reinterpret_cast<void**>(&index_data_begin)); // TODO: ThrowIfFailed
        memcpy(index_data_begin, &indices[0], index_buffer_size);
        new_box.index_buffer->Unmap(0, nullptr);
    }

    new_box.index_buffer_view.BufferLocation = new_box.index_buffer->GetGPUVirtualAddress();
    new_box.index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
    new_box.index_buffer_view.SizeInBytes = index_buffer_size;

    return new_box;
}
} // namespace ysn
