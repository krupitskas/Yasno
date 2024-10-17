#include "GltfLoader.hpp"

#include <memory>

#include <d3d12.h>
#include <wrl.h>
#include <d3dx12.h>
#include <wil/com.h>
#include <tiny_gltf.h>

#include <Renderer/DxRenderer.hpp>
#include <System/String.hpp>
#include <System/Math.hpp>
#include <System/Application.hpp>
#include <Graphics/RenderScene.hpp>
#include <Graphics/Primitive.hpp>
#include <Graphics/SurfaceMaterial.hpp>
#include <Renderer/GenerateMipsSystem.hpp>

using namespace Microsoft::WRL;

struct LoadGltfContext
{
	wil::com_ptr<ID3D12GraphicsCommandList4> copy_cmd_list;
	std::vector<wil::com_ptr<ID3D12Resource>> staging_resources;
};

static bool BuildBuffers(ysn::Model& model, LoadGltfContext& build_context, const tinygltf::Model& gltf_model)
{
	HRESULT hr = S_OK;

	auto dx_renderer = ysn::Application::Get().GetRenderer();

	model.buffers.reserve(gltf_model.buffers.size());

	for (const tinygltf::Buffer& buffer : gltf_model.buffers)
	{
		wil::com_ptr<ID3D12Resource> dst_buffer;
		{
			D3D12_HEAP_PROPERTIES heap_properties = {};
			heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
			heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			D3D12_RESOURCE_DESC resource_desc = {};
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resource_desc.Alignment = 0;
			resource_desc.Width = buffer.data.size();
			resource_desc.Height = 1;
			resource_desc.DepthOrArraySize = 1;
			resource_desc.MipLevels = 1;
			resource_desc.Format = DXGI_FORMAT_UNKNOWN;
			resource_desc.SampleDesc = { 1, 0 };
			resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			hr = dx_renderer->GetDevice()->CreateCommittedResource(&heap_properties,
																  D3D12_HEAP_FLAG_NONE,
																  &resource_desc,
																  D3D12_RESOURCE_STATE_COPY_DEST,
																  nullptr,
																  IID_PPV_ARGS(&dst_buffer));

		#ifndef YSN_RELEASE
			std::wstring name(buffer.name.begin(), buffer.name.end());
			name = name.empty() ? L"GLTF Buffer Dest" : name;
			dst_buffer->SetName(name.c_str());
		#endif

			if (hr != S_OK)
			{
				LogError << "Can't allocate GLTF dst buffer\n";
				return false;
			}
		}

		wil::com_ptr<ID3D12Resource> src_buffer;
		{
			D3D12_HEAP_PROPERTIES heap_properties = {};
			heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
			heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			D3D12_RESOURCE_DESC resource_desc = {};
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resource_desc.Alignment = 0;
			resource_desc.Width = buffer.data.size();
			resource_desc.Height = 1;
			resource_desc.DepthOrArraySize = 1;
			resource_desc.MipLevels = 1;
			resource_desc.Format = DXGI_FORMAT_UNKNOWN;
			resource_desc.SampleDesc = { 1, 0 };
			resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			hr = dx_renderer->GetDevice()->CreateCommittedResource(&heap_properties,
																  D3D12_HEAP_FLAG_NONE,
																  &resource_desc,
																  D3D12_RESOURCE_STATE_GENERIC_READ,
																  nullptr, IID_PPV_ARGS(&src_buffer));

			if (hr != S_OK)
			{
				LogError << "Can't allocate GLTF src buffer\n";
				return false;
			}

			build_context.staging_resources.push_back(src_buffer);

		#ifndef YSN_RELEASE
			std::wstring name(buffer.name.begin(), buffer.name.end());
			name = name.empty() ? L"GLTF Buffer Source" : name;
			src_buffer->SetName(name.c_str());
		#endif

			void* data_ptr;
			hr = src_buffer->Map(0, nullptr, &data_ptr);

			if (hr != S_OK)
			{
				LogError << "Can't map GLTF src buffer\n";
				return false;
			}

			// Copy cpu data to cpu src buffer
			memcpy(data_ptr, &buffer.data[0], buffer.data.size());
		}

		// Copy from CPU to GPU buffer
		build_context.copy_cmd_list->CopyBufferRegion(dst_buffer.get(), 0, src_buffer.get(), 0, buffer.data.size());

		model.buffers.push_back(dst_buffer);
	}

	return true;
}


static DXGI_FORMAT GetSrgbFormat(DXGI_FORMAT format)
{
	switch (format)
	{
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

		case DXGI_FORMAT_BC1_UNORM:
			return DXGI_FORMAT_BC1_UNORM_SRGB;

		case DXGI_FORMAT_BC2_UNORM:
			return DXGI_FORMAT_BC2_UNORM_SRGB;

		case DXGI_FORMAT_BC3_UNORM:
			return DXGI_FORMAT_BC3_UNORM_SRGB;

		case DXGI_FORMAT_B8G8R8A8_UNORM:
			return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

		case DXGI_FORMAT_B8G8R8X8_UNORM:
			return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

		case DXGI_FORMAT_BC7_UNORM:
			return DXGI_FORMAT_BC7_UNORM_SRGB;

		default:
			return format;
	}
}

// TODO: Move to ColorBuffer
// Compute the number of texture levels needed to reduce to 1x1.  This uses
// _BitScanReverse to find the highest set bit.  Each dimension reduces by
// half and truncates bits.  The dimension 256 (0x100) has 9 mip levels, same
// as the dimension 511 (0x1FF).
static inline uint32_t ComputeNumMips(uint32_t Width, uint32_t Height)
{
	uint32_t HighBit;
	_BitScanReverse((unsigned long*)&HighBit, Width | Height);
	return HighBit + 1;
}

static bool BuildImages(ysn::Model& model, LoadGltfContext& build_context, const tinygltf::Model& gltf_model)
{
	auto dx_renderer = ysn::Application::Get().GetRenderer();

	HRESULT hr = S_OK;

	for (const tinygltf::Image& image : gltf_model.images)
	{
		const uint32_t num_mips = ComputeNumMips(image.width, image.height);
		bool is_srgb = false; // TODO: add it back

		wil::com_ptr<ID3D12Resource> dst_texture;
		{
			D3D12_HEAP_PROPERTIES heap_properties = {};
			heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
			heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			// TODO: HDR textures?
			D3D12_RESOURCE_DESC resource_desc = {};
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resource_desc.Alignment = 0;
			resource_desc.Width = image.width;
			resource_desc.Height = image.height;
			resource_desc.DepthOrArraySize = 1;
			resource_desc.MipLevels = static_cast<UINT16>(num_mips);
			resource_desc.Format = is_srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM; // GetSrgbFormat
			resource_desc.SampleDesc = { 1, 0 };
			resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // FIXME: UAV / SRV?

			hr = dx_renderer->GetDevice()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&dst_texture));

			if (hr != S_OK)
			{
				LogError << "Can't allocate GLTF dst texture\n";
				return false;
			}

		#ifndef YSN_RELEASE
			std::wstring name(image.uri.begin(), image.uri.end());
			dst_texture->SetName(name.c_str());
		#endif

			ysn::GpuTexture new_texture;
			new_texture.name = std::wstring(image.uri.begin(), image.uri.end());
			new_texture.gpu_resource = dst_texture;
			new_texture.is_srgb = is_srgb;
			new_texture.num_mips = num_mips;
			new_texture.width = image.width;
			new_texture.height = image.height;

			model.textures.push_back(new_texture);
		}

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;

		UINT	row_count = 0;
		UINT64	row_size = 0;
		UINT64	size = 0;

		const D3D12_RESOURCE_DESC texture_desc = dst_texture->GetDesc();

		dx_renderer->GetDevice()->GetCopyableFootprints(&texture_desc, 0, 1, 0, &footprint, &row_count, &row_size, &size);

		wil::com_ptr<ID3D12Resource> src_resource;
		{
			D3D12_HEAP_PROPERTIES heap_properties = {};
			heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
			heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			D3D12_RESOURCE_DESC resource_desc = {};
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resource_desc.Alignment = 0;
			resource_desc.Width = size;
			resource_desc.Height = 1;
			resource_desc.DepthOrArraySize = 1;
			resource_desc.MipLevels = 1;
			resource_desc.Format = DXGI_FORMAT_UNKNOWN;
			resource_desc.SampleDesc = { 1, 0 };
			resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			hr = dx_renderer->GetDevice()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&src_resource));

			if (hr != S_OK)
			{
				LogError << "Can't allocate GLTF src texture resource\n";
				return false;
			}

			build_context.staging_resources.push_back(src_resource);

			void* data_ptr;
			hr = src_resource->Map(0, nullptr, &data_ptr);

			if (hr != S_OK)
			{
				LogError << "Can't map GLTF src texture resource\n";
				return false;
			}

			// Move data from gltf texture to resource
			for (UINT i = 0; i != row_count; i++)
			{
				memcpy(static_cast<uint8_t*>(data_ptr) + row_size * i, &image.image[0] + image.width * image.component * i, image.width * image.component);
			}

			// TODO: try to unmap here?
		}

		D3D12_TEXTURE_COPY_LOCATION dst_copy_location = {};
		dst_copy_location.pResource = dst_texture.get();
		dst_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst_copy_location.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION src_copy_location = {};
		src_copy_location.pResource = src_resource.get();
		src_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src_copy_location.PlacedFootprint = footprint;

		// Copy texture to GPU
		build_context.copy_cmd_list->CopyTextureRegion(&dst_copy_location, 0, 0, 0, &src_copy_location, nullptr);
	}

	return true;
}

//void FindAllSrgbTextures(ysn::ModelRenderContext* model_renderer_context, tinygltf::Model* gltf_model)
//{
//	for (const tinygltf::Material& gltf_material : gltf_model->materials)
//	{
//		const tinygltf::PbrMetallicRoughness& pbr_material = gltf_material.pbrMetallicRoughness;
//
//		{
//			const tinygltf::TextureInfo& base_color = pbr_material.baseColorTexture;
//
//			if (base_color.index >= 0)
//			{
//				const tinygltf::Texture& texture = gltf_model->textures[base_color.index];
//
//				if (model_renderer_context->srgb_textures.contains(texture.source))
//				{
//					LogError << "Base color texture sRGB search collision\n";
//				}
//
//				model_renderer_context->srgb_textures.emplace(texture.source);
//			}
//		}
//
//		{
//			const tinygltf::TextureInfo& emissive_texture = gltf_material.emissiveTexture;
//
//			if (emissive_texture.index >= 0)
//			{
//				const tinygltf::Texture& texture = gltf_model->textures[emissive_texture.index];
//
//				if (model_renderer_context->srgb_textures.contains(texture.source))
//				{
//					LogError << "Emissive texture sRGB search collision\n";
//				}
//
//				model_renderer_context->srgb_textures.emplace(texture.source);
//			}
//		}
//	}
//}

static void BuildSamplerDescs(ysn::Model& model, const tinygltf::Model& gltf_model)
{
	model.sampler_descs.reserve(gltf_model.samplers.size());

	for (const tinygltf::Sampler& gltf_sampler : gltf_model.samplers)
	{
		D3D12_SAMPLER_DESC sampler_desc = {};

		switch (gltf_sampler.minFilter)
		{
			case TINYGLTF_TEXTURE_FILTER_NEAREST:
				if (gltf_sampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
				{
					sampler_desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
				}
				else
				{
					sampler_desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				}
				break;
			case TINYGLTF_TEXTURE_FILTER_LINEAR:
				if (gltf_sampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
				{
					sampler_desc.Filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
				}
				else
				{
					sampler_desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
				}
				break;
			case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
				if (gltf_sampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
				{
					sampler_desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
				}
				else
				{
					sampler_desc.Filter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
				}
				break;
			case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
				if (gltf_sampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
				{
					sampler_desc.Filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
				}
				else
				{
					sampler_desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
				}
				break;
			case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
				if (gltf_sampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
				{
					sampler_desc.Filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
				}
				else
				{
					sampler_desc.Filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
				}
				break;
			case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
				if (gltf_sampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
				{
					sampler_desc.Filter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
				}
				else
				{
					sampler_desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				}
				break;
			default:
			{
				sampler_desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			}
			break;
		}

		auto to_texture_address_wrap = [](int wrap)
		{
			switch (wrap)
			{
				case TINYGLTF_TEXTURE_WRAP_REPEAT:
				{
					return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				}
				case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
				{
					return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				}
				case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
				{
					return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
				}
				default:
				{
					LogWarning << "GLTF sampler desc found incorrect wrap mode\n";
					return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				}
			}
		};

		sampler_desc.AddressU = to_texture_address_wrap(gltf_sampler.wrapS);
		sampler_desc.AddressV = to_texture_address_wrap(gltf_sampler.wrapT);
		sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler_desc.MaxLOD = 256;

		model.sampler_descs.push_back(sampler_desc);
	}
}

static bool BuildPrimitiveTopology(ysn::Primitive& primitive, int gltf_primitive_mode)
{
	switch (gltf_primitive_mode)
	{
		case TINYGLTF_MODE_POINTS:
			primitive.topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
			break;
		case TINYGLTF_MODE_LINE:
			primitive.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
			break;
		case TINYGLTF_MODE_LINE_STRIP:
			primitive.topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
			break;
		case TINYGLTF_MODE_TRIANGLES:
			primitive.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			break;
		case TINYGLTF_MODE_TRIANGLE_STRIP:
			primitive.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			break;
		default:
			LogError << "GLTF has unknown primitive topology\n";
			return false;
	}

	return true;
}

static void BuildIndexBuffer(ysn::Primitive& primitive, const ysn::Model& model, int indices_index, const tinygltf::Model& gltf_model)
{
	if (indices_index >= 0)
	{
		const tinygltf::Accessor& gltf_accessor = gltf_model.accessors[indices_index];
		const tinygltf::BufferView& gltf_buffer_view = gltf_model.bufferViews[gltf_accessor.bufferView];

		primitive.index_buffer_view.BufferLocation = model.buffers[gltf_buffer_view.buffer].GetGPUVirtualAddress() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
		primitive.index_buffer_view.SizeInBytes = static_cast<UINT>(gltf_buffer_view.byteLength - gltf_accessor.byteOffset);

		switch (gltf_accessor.componentType)
		{
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				primitive.index_buffer_view.Format = DXGI_FORMAT_R8_UINT;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				primitive.index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				primitive.index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
				break;
		}

		primitive.index_count = static_cast<uint32_t>(gltf_accessor.count);

		//pRenderPrimitive->index_buffer = pBuffers[gltf_buffer_view.buffer];
		//pRenderPrimitive->index_offset_in_bytes = gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
	}
	else
	{
		LogWarning << "GLTF primitive don't have indices\n";
	}
}

static void BuildAccessorType(ysn::Attribute& attribute, const int gltf_accessorType)
{
	switch (gltf_accessorType)
	{
		case TINYGLTF_TYPE_VEC2:
			attribute.format = DXGI_FORMAT_R32G32_FLOAT;
			break;
		case TINYGLTF_TYPE_VEC3:
			attribute.format = DXGI_FORMAT_R32G32B32_FLOAT;
			break;
		case TINYGLTF_TYPE_VEC4:
			attribute.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			break;
		default:;
	}
}

static void BuildAttributesAccessors(ysn::Primitive& primitive,
							  const ysn::Model& model,
							  const tinygltf::Model& gltf_model,
							  const std::map<std::string, int>& gltf_primitive_attributes)
{
	for (const auto& [gltf_attribute_name, gltf_accessor_index] : gltf_primitive_attributes)
	{
		const tinygltf::Accessor& gltf_accessor = gltf_model.accessors[gltf_accessor_index];
		const tinygltf::BufferView& gltf_buffer_view = gltf_model.bufferViews[gltf_accessor.bufferView];

		ysn::Attribute render_attribute;
		render_attribute.name = gltf_attribute_name;

		BuildAccessorType(render_attribute, gltf_accessor.type);

		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view = {};
		vertex_buffer_view.BufferLocation = model.buffers[gltf_buffer_view.buffer].GetGPUVirtualAddress() + gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
		vertex_buffer_view.SizeInBytes = static_cast<UINT>(gltf_buffer_view.byteLength - gltf_accessor.byteOffset);
		vertex_buffer_view.StrideInBytes = gltf_accessor.ByteStride(gltf_buffer_view);

		render_attribute.vertex_buffer_view = vertex_buffer_view;
		render_attribute.vertex_count = static_cast<uint32_t>(gltf_accessor.count);

		//if (gltf_attribute_name == "POSITION")
		//{
		//	primitive->vertex_count = = static_cast<uint32_t>(gltf_accessor.count);;
		//	primitive->vertex_buffer = pGltfBuffers[gltf_buffer_view.buffer];
		//	primitive->vertex_stride = render_attribute.vertexBufferView.StrideInBytes;
		//	primitive->vertex_offset_in_bytes = gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
		//}

		primitive.attributes.emplace(gltf_attribute_name, render_attribute);
	}
}

static void BuildMeshes(ysn::Model& model, const tinygltf::Model& gltf_model)
{
	for (const tinygltf::Mesh& gltf_mesh : gltf_model.meshes)
	{
		ysn::Mesh mesh;
		mesh.name = gltf_mesh.name;

		for (const tinygltf::Primitive& gltf_primitive : gltf_mesh.primitives)
		{
			ysn::Primitive primitive;

			BuildAttributesAccessors(primitive, model, gltf_model, gltf_primitive.attributes);
			BuildIndexBuffer(primitive, model, gltf_primitive.indices, gltf_model);
			BuildPrimitiveTopology(primitive, gltf_primitive.mode);

			primitive.material_id = gltf_primitive.material;

			mesh.primitives.push_back(primitive);
		}

		model.meshes.push_back(mesh);
	}
}


static bool BuildNodes(ysn::Model& model, const tinygltf::Model& gltf_model, const ysn::LoadingParameters& loading_parameters)
{
	auto dx_renderer = ysn::Application::Get().GetRenderer();

	HRESULT hr = S_OK;

	for (const tinygltf::Node& gltf_node : gltf_model.nodes)
	{
		D3D12_HEAP_PROPERTIES heap_properties = {};
		heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
		heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC resource_desc = {};
		resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resource_desc.Width = ysn::AlignPow2(sizeof(DirectX::XMFLOAT4X4), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		resource_desc.Height = 1;
		resource_desc.DepthOrArraySize = 1;
		resource_desc.MipLevels = 1;
		resource_desc.Format = DXGI_FORMAT_UNKNOWN;
		resource_desc.SampleDesc = { 1, 0 };
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		wil::com_ptr<ID3D12Resource> buffer;
		hr = dx_renderer->GetDevice()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer));

		if (hr != S_OK)
		{
			LogError << "GLTF Build Nodes can't allocate buffer\n";
			return false;
		}

		void* data_ptr;
		hr = buffer->Map(0, nullptr, &data_ptr);

		if (hr != S_OK)
		{
			LogError << "GLTF Build Nodes can't map buffer\n";
			return false;
		}

		if (gltf_node.matrix.empty())
		{
			XMStoreFloat4x4(static_cast<DirectX::XMFLOAT4X4*>(data_ptr), loading_parameters.model_modifier);

			model.transforms.push_back(loading_parameters.model_modifier);
		}
		else
		{
			if (gltf_node.matrix.size() != 16)
			{
				LogError << "GLTF node has broken matrix!\n";
				return false;
			}

			std::array<float, 16> float_array;

			// Convert to floats
			for (int i = 0; i < gltf_node.matrix.size(); i++)
			{
				float_array[0] = static_cast<float>(gltf_node.matrix[i]);
			}

			DirectX::XMMATRIX model_matrix(float_array.data());

			// Apply loading modifier
			model_matrix *= loading_parameters.model_modifier;

			XMStoreFloat4x4(static_cast<DirectX::XMFLOAT4X4*>(data_ptr), model_matrix);

			model.transforms.push_back(model_matrix);
		}

		model.node_buffers.push_back(buffer);
	}
}

static bool BuildMaterials(ysn::Model& model, LoadGltfContext& build_context, const tinygltf::Model& gltf_model)
{
	auto dx_renderer = ysn::Application::Get().GetRenderer();

	HRESULT hr = S_OK;

	for (const tinygltf::Material& gltf_material : gltf_model.materials)
	{
		ysn::Material material(gltf_material.name);

		if (gltf_material.alphaMode == "BLEND")
		{
			material.blend_desc.RenderTarget[0].BlendEnable = true;
			material.blend_desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			material.blend_desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			material.blend_desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			material.blend_desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			material.blend_desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
			material.blend_desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			material.blend_desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		}
		else if (gltf_material.alphaMode == "MASK")
		{
			// TODO(gltf): Check other alpha modes
			// assert( false );
		}
		else if (gltf_material.alphaMode == "OPAQUE")
		{
			material.blend_desc.RenderTarget[0].BlendEnable = false;

			// TODO(gltf): Check other alpha modes
			// assert( false );
		}

		material.blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		if (gltf_material.doubleSided)
		{
			material.rasterizer_desc.CullMode = D3D12_CULL_MODE_NONE;
		}
		else
		{
			material.rasterizer_desc.CullMode = D3D12_CULL_MODE_FRONT;
		}

		material.rasterizer_desc.FillMode = D3D12_FILL_MODE_SOLID;
		material.rasterizer_desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		material.rasterizer_desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		material.rasterizer_desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		material.rasterizer_desc.ForcedSampleCount = 0;
		material.rasterizer_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		D3D12_HEAP_PROPERTIES heap_properties = {};
		heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
		heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC resource_desc = {};
		resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resource_desc.Width = ysn::AlignPow2(sizeof(ysn::SurfaceShaderParameters), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		resource_desc.Height = 1;
		resource_desc.DepthOrArraySize = 1;
		resource_desc.MipLevels = 1;
		resource_desc.Format = DXGI_FORMAT_UNKNOWN;
		resource_desc.SampleDesc = { 1, 0 };
		resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		void* material_data_buffer;

		hr = dx_renderer->GetDevice()->CreateCommittedResource(
			&heap_properties,
			D3D12_HEAP_FLAG_NONE,
			&resource_desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&material.gpu_material_parameters.resource));

		if (hr != S_OK)
		{
			LogError << "GLTF build materials can't create buffer\n";
			return false;
		}

		hr = material.gpu_material_parameters.resource->Map(0, nullptr, &material_data_buffer);

		if (hr != S_OK)
		{
			LogError << "GLTF build materials can't map buffer\n";
			return false;
		}

		const tinygltf::PbrMetallicRoughness& gltf_pbr_material = gltf_material.pbrMetallicRoughness;
		ysn::SurfaceShaderParameters* shader_parameters = static_cast<ysn::SurfaceShaderParameters*>(material_data_buffer);

		DirectX::XMFLOAT4& base_color_factor = shader_parameters->base_color_factor;
		base_color_factor.x = static_cast<float>(gltf_pbr_material.baseColorFactor[0]);
		base_color_factor.y = static_cast<float>(gltf_pbr_material.baseColorFactor[1]);
		base_color_factor.z = static_cast<float>(gltf_pbr_material.baseColorFactor[2]);
		base_color_factor.w = static_cast<float>(gltf_pbr_material.baseColorFactor[3]);

		shader_parameters->metallic_factor = static_cast<float>(gltf_pbr_material.metallicFactor);
		shader_parameters->roughness_factor = static_cast<float>(gltf_pbr_material.roughnessFactor);

		if (gltf_pbr_material.baseColorTexture.index >= 0)
		{
			shader_parameters->texture_enable_bitmask |= 1 << ALBEDO_ENABLED_BIT;

			const tinygltf::Texture& gltf_texture = gltf_model.textures[gltf_pbr_material.baseColorTexture.index];

			// TODO: Preallocate 5 handles and provide them through array, do not save first handle
			const auto srv_handle = dx_renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
			const auto sampler_handle = dx_renderer->GetSamplerDescriptorHeap()->GetNewHandle();

			ysn::GpuTexture& texture = model.textures[gltf_texture.source]; // TODO: use it for srgb?
			texture.descriptor_handle = srv_handle;
			texture.is_srgb = true;

			shader_parameters->albedo_texture_index = dx_renderer->GetCbvSrvUavDescriptorHeap()->GetDescriptorIndex(srv_handle);

			D3D12_RESOURCE_DESC texture_desc = texture.gpu_resource->GetDesc();

		#ifndef YSN_RELEASE
			texture.gpu_resource->SetName(L"Albedo Texture");
		#endif

			texture.name = L"Albedo Texture";

			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Format = texture_desc.Format;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = texture_desc.MipLevels;

			dx_renderer->GetDevice()->CreateShaderResourceView(texture.gpu_resource.get(), &srv_desc, srv_handle.cpu);

			//D3D12_SAMPLER_DESC& SamplerDesc = pModelRenderContext->SamplerDescs[GltfTexture.sampler];
			//p_renderer->GetDevice()->CreateSampler(&SamplerDesc, RenderMaterial.sampler_handle.cpu);
		}

		if (gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index > 0)
		{
			shader_parameters->texture_enable_bitmask |= 1 << METALLIC_ROUGHNESS_ENABLED_BIT;

			const tinygltf::Texture& gltfTexture = gltf_model.textures[gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index];
			ysn::GpuTexture& texture = model.textures[gltfTexture.source];

			const auto srv_handle = dx_renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
			const auto sampler_handle = dx_renderer->GetSamplerDescriptorHeap()->GetNewHandle();

			texture.descriptor_handle = srv_handle;
			shader_parameters->metallic_roughness_texture_index = dx_renderer->GetCbvSrvUavDescriptorHeap()->GetDescriptorIndex(srv_handle);

		#ifndef YSN_RELEASE
			texture.gpu_resource->SetName(L"Metallic Roughness Texture");
		#endif

			texture.name = L"Metallic Roughness Texture";

			D3D12_RESOURCE_DESC texture_desc = texture.gpu_resource->GetDesc();

			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Format = texture_desc.Format;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = texture_desc.MipLevels;

			dx_renderer->GetDevice()->CreateShaderResourceView(texture.gpu_resource.get(), &srv_desc, srv_handle.cpu);

			//auto& samplerDesc = pModelRenderContext->SamplerDescs[gltfTexture.sampler];
			//p_renderer->GetDevice()->CreateSampler(&samplerDesc, SamplerDescriptor);
		}

		if (gltf_material.normalTexture.index > 0)
		{
			shader_parameters->texture_enable_bitmask |= 1 << NORMAL_ENABLED_BIT;

			const tinygltf::Texture& gltf_texture = gltf_model.textures[gltf_material.normalTexture.index];
			ysn::GpuTexture& texture = model.textures[gltf_texture.source];

			const auto srv_handle = dx_renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
			const auto sampler_handle = dx_renderer->GetSamplerDescriptorHeap()->GetNewHandle();

			shader_parameters->normal_texture_index = dx_renderer->GetCbvSrvUavDescriptorHeap()->GetDescriptorIndex(srv_handle);

			texture.descriptor_handle = srv_handle;

		#ifndef YSN_RELEASE
			texture.gpu_resource->SetName(L"Normals Texture");
		#endif

			texture.name = L"Normals Texture";

			D3D12_RESOURCE_DESC texture_desc = texture.gpu_resource->GetDesc();

			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Format = texture_desc.Format;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = texture_desc.MipLevels;

			dx_renderer->GetDevice()->CreateShaderResourceView(texture.gpu_resource.get(), &srv_desc, srv_handle.cpu);

			//D3D12_SAMPLER_DESC& SamplerDesc = pModelRenderContext->SamplerDescs[GltfTexture.sampler];
			//p_renderer->GetDevice()->CreateSampler(&SamplerDesc, SamplerDescriptor);
		}

		if (gltf_material.occlusionTexture.index > 0)
		{
			shader_parameters->texture_enable_bitmask |= 1 << OCCLUSION_ENABLED_BIT;

			const tinygltf::Texture& gltf_texture = gltf_model.textures[gltf_material.occlusionTexture.index];
			ysn::GpuTexture& texture = model.textures[gltf_texture.source];

			const auto srv_handle = dx_renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
			const auto sampler_handle = dx_renderer->GetSamplerDescriptorHeap()->GetNewHandle();

			texture.descriptor_handle = srv_handle;

			shader_parameters->occlusion_texture_index = dx_renderer->GetCbvSrvUavDescriptorHeap()->GetDescriptorIndex(srv_handle);

		#ifndef YSN_RELEASE
			texture.gpu_resource->SetName(L"Occlusion Texture");
		#endif

			texture.name = L"Occlusion Texture";

			D3D12_RESOURCE_DESC texture_desc = texture.gpu_resource->GetDesc();

			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Format = texture_desc.Format;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = texture_desc.MipLevels;

			dx_renderer->GetDevice()->CreateShaderResourceView(texture.gpu_resource.get(), &srv_desc, srv_handle.cpu);

			//D3D12_SAMPLER_DESC& samplerDesc = pModelRenderContext->SamplerDescs[gltfTexture.sampler];
			//p_renderer->GetDevice()->CreateSampler(&samplerDesc, SamplerDescriptor);
		}

		if (gltf_material.emissiveTexture.index > 0)
		{
			shader_parameters->texture_enable_bitmask |= 1 << EMISSIVE_ENABLED_BIT;

			const tinygltf::Texture& gltf_texture = gltf_model.textures[gltf_material.emissiveTexture.index];
			ysn::GpuTexture& texture = model.textures[gltf_texture.source];
			texture.is_srgb = true;

			const auto srv_handle = dx_renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
			const auto sampler_handle = dx_renderer->GetSamplerDescriptorHeap()->GetNewHandle();

			shader_parameters->emissive_texture_index = dx_renderer->GetCbvSrvUavDescriptorHeap()->GetDescriptorIndex(srv_handle);

			texture.descriptor_handle = srv_handle;

		#ifndef YSN_RELEASE
			texture.gpu_resource->SetName(L"Emissive Texture");
		#endif

			texture.name = L"Emissive Texture";

			D3D12_RESOURCE_DESC texture_desc = texture.gpu_resource->GetDesc();

			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Format = texture_desc.Format;
			srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = texture_desc.MipLevels;

			dx_renderer->GetDevice()->CreateShaderResourceView(texture.gpu_resource.get(), &srv_desc, srv_handle.cpu);

			//D3D12_SAMPLER_DESC& samplerDesc = pModelRenderContext->SamplerDescs[gltfTexture.sampler];
			//p_renderer->GetDevice()->CreateSampler(&samplerDesc, SamplerDescriptor);
		}

		material.gpu_material_parameters.resource->Unmap(0, nullptr);

		model.materials.push_back(material);
	}
}

namespace ysn
{
	bool ReadModel(Model& model, const tinygltf::Model& gltf_model, const LoadingParameters& loading_parameters)
	{
		auto dx_renderer = Application::Get().GetRenderer();
		auto command_queue = Application::Get().GetDirectQueue();

		LoadGltfContext load_gltf_context;
		load_gltf_context.staging_resources.reserve(256);
		load_gltf_context.copy_cmd_list = command_queue->GetCommandList("GLTF upload");

		if(!BuildBuffers(model, load_gltf_context, gltf_model))
		{
			LogError << "GLTF loader can't build buffers\n";
			return false;
		}
		if(!BuildImages(model, load_gltf_context, gltf_model))
		{
			LogError << "GLTF loader can't build materials\n";
			return false;
		}
		//FindAllSrgbTextures(ModelRenderContext, pModel);
		if(!BuildMaterials(model, load_gltf_context, gltf_model))
		{
			LogError << "GLTF loader can't build materials\n";
			return false;
		}
		BuildSamplerDescs(model, gltf_model);
		BuildMeshes(model, gltf_model);
		if(!BuildNodes(model, gltf_model, loading_parameters))
		{
			LogError << "GLTF loader can't build nodes\n";
			return false;
		}

		// Build pipelines
		// Compute mips 
		for(const GpuTexture& texture : model.textures)
		{
			dx_renderer->GetMipGenerator()->GenerateMips(dx_renderer, load_gltf_context.copy_cmd_list, texture);
		}

		auto fence_value = command_queue->ExecuteCommandList(load_gltf_context.copy_cmd_list);
		command_queue->WaitForFenceValue(fence_value);

		return true;
	}

	bool LoadGltfFromFile(RenderScene& render_scene, const std::wstring& load_path, const LoadingParameters& loading_parameters)
	{
		tinygltf::Model gltf_model;
		tinygltf::TinyGLTF gltf_loader;

		std::string error_str;
		std::string warning_str;
		const std::string load_path_str = ysn::WStringToString(load_path);

		const bool result = gltf_loader.LoadASCIIFromFile(&gltf_model, &error_str, &warning_str, load_path_str.c_str());

		LogInfo << "GLTF loading: " << load_path_str << "\n";

		if (!warning_str.empty())
		{
			LogWarning << "GLTF loading: " << warning_str.c_str() << "\n";
		}

		if (!error_str.empty())
		{
			LogError << "GLTF loading: " << error_str.c_str() << "\n";
		}

		if (!result)
		{
			LogError << "Failed to read: " << load_path_str << "\n";
			return false;
		}

		Model model;

		if(!ReadModel(model, gltf_model, loading_parameters))
		{
			LogInfo << "GLTF can't load model\n";
			return false;
		}

		render_scene.models.push_back(model);

		LogInfo << "GLTF loaded successfully: " << load_path_str << "\n";

		return true;
	}
}

