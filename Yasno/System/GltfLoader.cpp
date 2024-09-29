#include "GltfLoader.hpp"

#include <d3d12.h>
#include <wrl.h>

#include <dxcapi.h>

#include <Renderer/DxRenderer.hpp>
#include <Graphics/Techniques/ShadowMapPass.hpp>
#include <Renderer/GenerateMipsSystem.hpp>
#include <System/Filesystem.hpp>
#include <System/String.hpp>
#include <System/Math.hpp>

using namespace Microsoft::WRL;

/*

void BuildBuffers(
	ysn::ModelRenderContext* pModelRenderContext,
	std::shared_ptr<ysn::DxRenderer> p_renderer,
	wil::com_ptr<ID3D12GraphicsCommandList> pCopyCommandList,
	tinygltf::Model* pModel,
	std::vector<wil::com_ptr<ID3D12Resource>>* pStagingResources)
{
	HRESULT hr = S_OK;

	for (tinygltf::Buffer& buffer : pModel->buffers)
	{
		// wil::com_ptr<ID3D12Resource> pDstBuffer;
		//{
		//	D3D12_HEAP_PROPERTIES heapProperties = {};
		//	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		//	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		//	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		//	D3D12_RESOURCE_DESC resourceDesc = {};
		//	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		//	resourceDesc.Alignment = 0;
		//	resourceDesc.Width = buffer.data.size();
		//	resourceDesc.Height = 1;
		//	resourceDesc.DepthOrArraySize = 1;
		//	resourceDesc.MipLevels = 1;
		//	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		//	resourceDesc.SampleDesc = { 1, 0 };
		//	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		//	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		//	hr = p_renderer->GetDevice()->CreateCommittedResource(&heapProperties,
		//														  D3D12_HEAP_FLAG_NONE,
		//														  &resourceDesc,
		//														  D3D12_RESOURCE_STATE_COMMON, // D3D12_RESOURCE_STATE_COPY_DEST
		//														  nullptr,
		//														  IID_PPV_ARGS(&pDstBuffer));

		// #ifdef YSN_PROFILE
		//	std::wstring name(buffer.name.begin(), buffer.name.end());
		//	name = name.empty() ? L"Gltf_Buffer" : name;
		//	pDstBuffer->SetName(name.c_str());
		// #endif

		//	assert(SUCCEEDED(hr));

		//}

		wil::com_ptr<ID3D12Resource> pSrcBuffer;
		{
			D3D12_HEAP_PROPERTIES heapProperties = {};
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			D3D12_RESOURCE_DESC resourceDesc = {};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = buffer.data.size();
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc = { 1, 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			hr = p_renderer->GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&pSrcBuffer));

			assert(SUCCEEDED(hr));
			pStagingResources->push_back(pSrcBuffer);

		#ifndef YSN_RELEASE
			std::wstring name(buffer.name.begin(), buffer.name.end());
			name = name.empty() ? L"Gltf_Buffer" : name;
			pSrcBuffer->SetName(name.c_str());
		#endif

			void* pData;
			hr = pSrcBuffer->Map(0, nullptr, &pData);
			assert(SUCCEEDED(hr));

			memcpy(pData, &buffer.data[0], buffer.data.size());
		}

		pModelRenderContext->pBuffers.push_back(pSrcBuffer);

		// pCopyCommandList->CopyBufferRegion(pDstBuffer.get(), 0, pSrcBuffer.get(), 0, buffer.data.size());
	}
}

void BuildSamplerDescs(ysn::ModelRenderContext* pModelRenderContext, tinygltf::Model* pGltfModel)
{
	for (tinygltf::Sampler& GltfSampler : pGltfModel->samplers)
	{
		D3D12_SAMPLER_DESC SamplerDesc = {};
		switch (GltfSampler.minFilter)
		{
			case TINYGLTF_TEXTURE_FILTER_NEAREST:
				if (GltfSampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
				{
					SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
				}
				else
				{
					SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				}
				break;
			case TINYGLTF_TEXTURE_FILTER_LINEAR:
				if (GltfSampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
					SamplerDesc.Filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
				else
					SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
				break;
			case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
				if (GltfSampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
					SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
				else
					SamplerDesc.Filter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
				break;
			case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
				if (GltfSampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
					SamplerDesc.Filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
				else
					SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
				break;
			case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
				if (GltfSampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
				{
					SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
				}
				else
				{
					SamplerDesc.Filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
				}
				break;
			case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
				if (GltfSampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST)
				{
					SamplerDesc.Filter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
				}
				else
				{
					SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				}
				break;
			default:
				SamplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
				break;
		}

		auto toTextureAddressMode = [](int wrap)
		{
			switch (wrap)
			{
				case TINYGLTF_TEXTURE_WRAP_REPEAT:
					return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
					return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
					return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
				default:
					assert(false);
					return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			}
		};

		SamplerDesc.AddressU = toTextureAddressMode(GltfSampler.wrapS);
		SamplerDesc.AddressV = toTextureAddressMode(GltfSampler.wrapT);
		SamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		SamplerDesc.MaxLOD = 256; // TODO: eh??

		pModelRenderContext->SamplerDescs.push_back(SamplerDesc);
	}
}

void FindAllSrgbTextures(ysn::ModelRenderContext* model_renderer_context, tinygltf::Model* gltf_model)
{
	for (const tinygltf::Material& gltf_material : gltf_model->materials)
	{
		const tinygltf::PbrMetallicRoughness& pbr_material = gltf_material.pbrMetallicRoughness;

		{
			const tinygltf::TextureInfo& base_color = pbr_material.baseColorTexture;

			if (base_color.index >= 0)
			{
				const tinygltf::Texture& texture = gltf_model->textures[base_color.index];

				if (model_renderer_context->srgb_textures.contains(texture.source))
				{
					LogError << "Base color texture sRGB search collision\n";
				}

				model_renderer_context->srgb_textures.emplace(texture.source);
			}
		}

		{
			const tinygltf::TextureInfo& emissive_texture = gltf_material.emissiveTexture;

			if (emissive_texture.index >= 0)
			{
				const tinygltf::Texture& texture = gltf_model->textures[emissive_texture.index];

				if (model_renderer_context->srgb_textures.contains(texture.source))
				{
					LogError << "Emissive texture sRGB search collision\n";
				}

				model_renderer_context->srgb_textures.emplace(texture.source);
			}
		}
	}
}

void BuildMaterials(ysn::ModelRenderContext* pModelRenderContext, std::shared_ptr<ysn::DxRenderer> p_renderer, wil::com_ptr<ID3D12GraphicsCommandList> pCopyCommandList, tinygltf::Model* pGltfModel)
{
	HRESULT hr = S_OK;

	for (tinygltf::Material& GltfMaterial : pGltfModel->materials)
	{
		ysn::Material RenderMaterial = {};

		// Set a material name.
		RenderMaterial.name = GltfMaterial.name;

		// Set a blend desc.
		D3D12_BLEND_DESC& blendDesc = RenderMaterial.blendDesc;
		if (GltfMaterial.alphaMode == "BLEND") // TODO: hide strings under constants
		{
			// TODO: check if this is only equation?
			blendDesc.RenderTarget[0].BlendEnable = true;
			blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		}
		else if (GltfMaterial.alphaMode == "MASK")
		{
			// TODO(gltf): Check other alpha modes
			// assert( false );
		}
		else if (GltfMaterial.alphaMode == "OPAQUE")
		{
			// TODO(gltf): Check other alpha modes
			// assert( false );
		}

		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		// Set a rasterizer desc.
		D3D12_RASTERIZER_DESC& rasterizerDesc = RenderMaterial.rasterizerDesc;
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

		if (GltfMaterial.doubleSided)
		{
			rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
		}
		else
		{
			rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;
		}

		// TODO: Why counter clockwise?
		// rasterizerDesc.FrontCounterClockwise = true;
		rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasterizerDesc.ForcedSampleCount = 0;
		rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		{
			D3D12_HEAP_PROPERTIES heapProperties = {};
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			D3D12_RESOURCE_DESC resourceDesc = {};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Width = ysn::AlignPow2(sizeof(ysn::PBRMetallicRoughnessShader), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc = { 1, 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			hr = p_renderer->GetDevice()->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&RenderMaterial.pBuffer));
			assert(SUCCEEDED(hr));

			hr = RenderMaterial.pBuffer->Map(0, nullptr, reinterpret_cast<void**>(&RenderMaterial.pBufferData));

			assert(SUCCEEDED(hr));
		}

		tinygltf::PbrMetallicRoughness& glTFPBRMetallicRoughness = GltfMaterial.pbrMetallicRoughness;
		ysn::PBRMetallicRoughnessShader* pPBRMetallicRoughness = static_cast<ysn::PBRMetallicRoughnessShader*>(RenderMaterial.pBufferData);

		{
			DirectX::XMFLOAT4& baseColorFactor = pPBRMetallicRoughness->baseColorFactor;

			baseColorFactor.x = static_cast<float>(glTFPBRMetallicRoughness.baseColorFactor[0]);
			baseColorFactor.y = static_cast<float>(glTFPBRMetallicRoughness.baseColorFactor[1]);
			baseColorFactor.z = static_cast<float>(glTFPBRMetallicRoughness.baseColorFactor[2]);
			baseColorFactor.w = static_cast<float>(glTFPBRMetallicRoughness.baseColorFactor[3]);

			tinygltf::TextureInfo& glTFBaseColorTexture = glTFPBRMetallicRoughness.baseColorTexture;

			if (glTFBaseColorTexture.index >= 0)
			{
				pPBRMetallicRoughness->texture_enable_bitmask |= 1 << ALBEDO_ENABLED_BITMASK;
			}

			pPBRMetallicRoughness->metallicFactor = static_cast<float>(glTFPBRMetallicRoughness.metallicFactor);
			pPBRMetallicRoughness->roughnessFactor = static_cast<float>(glTFPBRMetallicRoughness.roughnessFactor);

			tinygltf::TextureInfo& glTFMetallicRoughnessTexture = glTFPBRMetallicRoughness.metallicRoughnessTexture;

			if (glTFMetallicRoughnessTexture.index >= 0)
			{
				pPBRMetallicRoughness->texture_enable_bitmask |= 1 << METALLIC_ROUGHNESS_ENABLED_BITMASK;

			}

			tinygltf::NormalTextureInfo& GlTFNormalsTexture = GltfMaterial.normalTexture;

			if (GlTFNormalsTexture.index >= 0)
			{
				pPBRMetallicRoughness->texture_enable_bitmask |= 1 << NORMAL_ENABLED_BITMASK;
			}

			tinygltf::OcclusionTextureInfo& GlTFOcclusionTexture = GltfMaterial.occlusionTexture;

			if (GlTFOcclusionTexture.index >= 0)
			{
				pPBRMetallicRoughness->texture_enable_bitmask |= 1 << OCCLUSION_ENABLED_BITMASK;

			}

			auto& GlTFEmissiveTexture = GltfMaterial.emissiveTexture;

			if (GlTFEmissiveTexture.index >= 0)
			{
				pPBRMetallicRoughness->texture_enable_bitmask |= 1 << EMISSIVE_ENABLED_BITMASK;
			}
		}

		// TODO: Improve texture material handling

		// Albedo
		tinygltf::TextureInfo& glTFBaseColorTexture = glTFPBRMetallicRoughness.baseColorTexture;
		if (glTFBaseColorTexture.index >= 0)
		{
			const tinygltf::Texture& GltfTexture = pGltfModel->textures[glTFBaseColorTexture.index];

			// TODO: Preallocate 5 handles and provide them through array, do not save first handle
			RenderMaterial.srv_handle = p_renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
			RenderMaterial.sampler_handle = p_renderer->GetSamplerDescriptorHeap()->GetNewHandle();

			ysn::Texture& texture = pModelRenderContext->pTextures[GltfTexture.source];
			texture.descriptor_handle = RenderMaterial.srv_handle;

			pPBRMetallicRoughness->albedo_texture_index = p_renderer->GetCbvSrvUavDescriptorHeap()->GetDescriptorIndex(RenderMaterial.srv_handle);

			auto resource = texture.gpuTexture;
			auto resource_desc = resource->GetDesc();

			resource->SetName(L"Albedo");
			texture.name = L"Albedo";

			D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.Format						= resource_desc.Format;
			srv_desc.Shader4ComponentMapping	= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv_desc.ViewDimension				= D3D12_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels		= resource_desc.MipLevels;

			p_renderer->GetDevice()->CreateShaderResourceView(resource.get(), &srv_desc, RenderMaterial.srv_handle.cpu);
			
			D3D12_SAMPLER_DESC& SamplerDesc = pModelRenderContext->SamplerDescs[GltfTexture.sampler];
			p_renderer->GetDevice()->CreateSampler(&SamplerDesc, RenderMaterial.sampler_handle.cpu);
		}

		tinygltf::TextureInfo& glTFMetallicRoughnessTexture = GltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture;
		if (glTFMetallicRoughnessTexture.index > 0)
		{
			tinygltf::Texture& gltfTexture = pGltfModel->textures[glTFMetallicRoughnessTexture.index];
			auto& texture = pModelRenderContext->pTextures[gltfTexture.source];

			auto SrvDescriptor = p_renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();
			CD3DX12_CPU_DESCRIPTOR_HANDLE SamplerDescriptor = p_renderer->GetSamplerDescriptorHeap()->GetNewHandle().cpu;

			texture.descriptor_handle = SrvDescriptor;
			pPBRMetallicRoughness->metallic_roughness_texture_index = p_renderer->GetCbvSrvUavDescriptorHeap()->GetDescriptorIndex(SrvDescriptor);

			texture.gpuTexture->SetName(L"MetallicRoughness");

			p_renderer->GetDevice()->CreateShaderResourceView(texture.gpuTexture.get(), nullptr, SrvDescriptor.cpu);

			auto& samplerDesc = pModelRenderContext->SamplerDescs[gltfTexture.sampler];
			p_renderer->GetDevice()->CreateSampler(&samplerDesc, SamplerDescriptor);
		}

		// Normal
		// TODO: add scale
		tinygltf::NormalTextureInfo& gltfNormalTexture = GltfMaterial.normalTexture;
		if (gltfNormalTexture.index > 0)
		{
			const tinygltf::Texture& GltfTexture = pGltfModel->textures[gltfNormalTexture.index];
			ID3D12Resource* pTexture = pModelRenderContext->pTextures[GltfTexture.source].gpuTexture.get();

			auto descriptor = p_renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

			CD3DX12_CPU_DESCRIPTOR_HANDLE SrvDescriptor = descriptor.cpu;
			CD3DX12_CPU_DESCRIPTOR_HANDLE SamplerDescriptor = p_renderer->GetSamplerDescriptorHeap()->GetNewHandle().cpu;

			pPBRMetallicRoughness->normal_texture_index = p_renderer->GetCbvSrvUavDescriptorHeap()->GetDescriptorIndex(descriptor);

			pTexture->SetName(L"Normal");

			p_renderer->GetDevice()->CreateShaderResourceView(pTexture, nullptr, SrvDescriptor);

			D3D12_SAMPLER_DESC& SamplerDesc = pModelRenderContext->SamplerDescs[GltfTexture.sampler];
			p_renderer->GetDevice()->CreateSampler(&SamplerDesc, SamplerDescriptor);
		}

		// Occlusion
		tinygltf::OcclusionTextureInfo& gltfOcclusionTexture = GltfMaterial.occlusionTexture;
		if (gltfOcclusionTexture.index > 0)
		{
			tinygltf::Texture& gltfTexture = pGltfModel->textures[gltfOcclusionTexture.index];
			ID3D12Resource* pTexture = pModelRenderContext->pTextures[gltfTexture.source].gpuTexture.get();

			auto descriptor = p_renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

			CD3DX12_CPU_DESCRIPTOR_HANDLE SrvDescriptor = descriptor.cpu;
			CD3DX12_CPU_DESCRIPTOR_HANDLE SamplerDescriptor = p_renderer->GetSamplerDescriptorHeap()->GetNewHandle().cpu;

			pPBRMetallicRoughness->occlusion_texture_index = p_renderer->GetCbvSrvUavDescriptorHeap()->GetDescriptorIndex(descriptor);

			p_renderer->GetDevice()->CreateShaderResourceView(pTexture, nullptr, SrvDescriptor);

			D3D12_SAMPLER_DESC& samplerDesc = pModelRenderContext->SamplerDescs[gltfTexture.sampler];
			p_renderer->GetDevice()->CreateSampler(&samplerDesc, SamplerDescriptor);
		}

		// Emissive
		tinygltf::TextureInfo& gltfEmissiveTexture = GltfMaterial.emissiveTexture;
		if (gltfEmissiveTexture.index > 0)
		{
			tinygltf::Texture& gltfTexture = pGltfModel->textures[gltfEmissiveTexture.index];
			ID3D12Resource* pTexture = pModelRenderContext->pTextures[gltfTexture.source].gpuTexture.get();

			auto descriptor = p_renderer->GetCbvSrvUavDescriptorHeap()->GetNewHandle();

			CD3DX12_CPU_DESCRIPTOR_HANDLE SrvDescriptor = descriptor.cpu;
			CD3DX12_CPU_DESCRIPTOR_HANDLE SamplerDescriptor = p_renderer->GetSamplerDescriptorHeap()->GetNewHandle().cpu;

			pPBRMetallicRoughness->emissive_texture_index = p_renderer->GetCbvSrvUavDescriptorHeap()->GetDescriptorIndex(descriptor);

			p_renderer->GetDevice()->CreateShaderResourceView(pTexture, nullptr, SrvDescriptor);

			D3D12_SAMPLER_DESC& samplerDesc = pModelRenderContext->SamplerDescs[gltfTexture.sampler];
			p_renderer->GetDevice()->CreateSampler(&samplerDesc, SamplerDescriptor);
		}

		// Generate mips
		if (glTFBaseColorTexture.index >= 0)
		{
			const tinygltf::Texture& GltfTexture = pGltfModel->textures[glTFBaseColorTexture.index];
			auto& texture = pModelRenderContext->pTextures[GltfTexture.source];
			p_renderer->GetMipGenerator()->GenerateMips(p_renderer, pCopyCommandList, texture);
		}

		//if (glTFMetallicRoughnessTexture.index > 0)
		//{
		//	tinygltf::Texture& gltfTexture = pGltfModel->textures[glTFMetallicRoughnessTexture.index];
		//	auto& texture = pModelRenderContext->pTextures[gltfTexture.source];
		//	p_renderer->GetMipGenerator()->GenerateMips(p_renderer, pCopyCommandList, texture);
		//}

		//if (gltfNormalTexture.index > 0)
		//{
		//	const tinygltf::Texture& GltfTexture = pGltfModel->textures[gltfNormalTexture.index];
		//	auto& texture = pModelRenderContext->pTextures[GltfTexture.source];
		//	p_renderer->GetMipGenerator()->GenerateMips(p_renderer, pCopyCommandList, texture);
		//}

		//if (gltfOcclusionTexture.index > 0)
		//{
		//	tinygltf::Texture& gltfTexture = pGltfModel->textures[gltfOcclusionTexture.index];
		//	auto& texture = pModelRenderContext->pTextures[gltfTexture.source];
		//	p_renderer->GetMipGenerator()->GenerateMips(p_renderer, pCopyCommandList, texture);
		//}

		//if (gltfEmissiveTexture.index > 0)
		//{
		//	tinygltf::Texture& gltfTexture = pGltfModel->textures[gltfEmissiveTexture.index];
		//	auto& texture = pModelRenderContext->pTextures[gltfTexture.source];
		//	p_renderer->GetMipGenerator()->GenerateMips(p_renderer, pCopyCommandList, texture);
		//}

		pModelRenderContext->Materials.push_back(RenderMaterial);
	}
}

// TODO: cache it
std::vector<DxcDefine> BuildAttributeDefines(const std::vector<ysn::Attribute>& Attributes)
{
	std::vector<DxcDefine> Defines;

	for (const ysn::Attribute& Attribute : Attributes)
	{
		if (Attribute.name == "NORMAL")
		{
			Defines.push_back({ L"HAS_NORMAL", L"1" });
		}
		else if (Attribute.name == "TANGENT")
		{
			Defines.push_back({ L"HAS_TANGENT", L"1" });
		}
		else if (Attribute.name == "TEXCOORD_0")
		{
			Defines.push_back({ L"HAS_TEXCOORD_0", L"1" });
		}
		else if (Attribute.name == "TEXCOORD_1")
		{
			Defines.push_back({ L"HAS_TEXCOORD_1", L"1" });
		}
	}

	return Defines;
}

std::vector<D3D12_INPUT_ELEMENT_DESC> BuildInputElementDescs(const std::vector<ysn::Attribute>& RenderAttributes)
{
	std::vector<D3D12_INPUT_ELEMENT_DESC> InputElementDescs;

	for (const ysn::Attribute& RenderAttribute : RenderAttributes)
	{
		D3D12_INPUT_ELEMENT_DESC InputElementDesc = {};
		InputElementDesc.SemanticName = &RenderAttribute.name[0];
		InputElementDesc.Format = RenderAttribute.format;

		// TODO: Need to parse semantic name and index from attribute name.
		if (RenderAttribute.name == "TEXCOORD_0")
		{
			InputElementDesc.SemanticName = "TEXCOORD_";
			InputElementDesc.SemanticIndex = 0;
		}

		if (RenderAttribute.name == "TEXCOORD_1")
		{
			InputElementDesc.SemanticName = "TEXCOORD_";
			InputElementDesc.SemanticIndex = 1;
		}

		if (RenderAttribute.name == "TEXCOORD_2")
		{
			InputElementDesc.SemanticName = "TEXCOORD_";
			InputElementDesc.SemanticIndex = 2;
		}

		if (RenderAttribute.name == "COLOR_0")
		{
			InputElementDesc.SemanticName = "COLOR_";
			InputElementDesc.SemanticIndex = 0;
		}

		if (RenderAttribute.name == "COLOR_1")
		{
			InputElementDesc.SemanticName = "COLOR_";
			InputElementDesc.SemanticIndex = 1;
		}

		InputElementDesc.InputSlot = static_cast<UINT>(InputElementDescs.size());
		InputElementDesc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		InputElementDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		InputElementDescs.push_back(InputElementDesc);
	}

	return InputElementDescs;
}

bool ForwardPipeline(ysn::Primitive* pRenderPrimitive, std::shared_ptr<ysn::DxRenderer> renderer)
{
	HRESULT result = S_OK;
	{
		D3D12_DESCRIPTOR_RANGE SrvDescriptorRange = {};
		SrvDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		SrvDescriptorRange.NumDescriptors = 5;
		SrvDescriptorRange.BaseShaderRegister = 0;

		D3D12_DESCRIPTOR_RANGE DepthInputDescriptorRange = {};
		DepthInputDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		DepthInputDescriptorRange.NumDescriptors = 1;
		DepthInputDescriptorRange.BaseShaderRegister = 1 + 5;

		D3D12_ROOT_PARAMETER srv_range;
		srv_range.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		srv_range.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		srv_range.DescriptorTable.NumDescriptorRanges = 1;
		srv_range.DescriptorTable.pDescriptorRanges = &SrvDescriptorRange;

		D3D12_ROOT_PARAMETER depth_range;
		depth_range.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		depth_range.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		depth_range.DescriptorTable.NumDescriptorRanges = 1;
		depth_range.DescriptorTable.pDescriptorRanges = &DepthInputDescriptorRange;

		D3D12_ROOT_PARAMETER rootParams[6] = {
			{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 0, 0 }, D3D12_SHADER_VISIBILITY_ALL },
			{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 1, 0 }, D3D12_SHADER_VISIBILITY_VERTEX },
			{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 2, 0 }, D3D12_SHADER_VISIBILITY_PIXEL },
			{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 3, 0 }, D3D12_SHADER_VISIBILITY_ALL },
			srv_range,
			depth_range
		};

		// TEMP
		rootParams[0].Descriptor.RegisterSpace = 0;
		rootParams[1].Descriptor.RegisterSpace = 0;
		rootParams[2].Descriptor.RegisterSpace = 0;
		rootParams[3].Descriptor.RegisterSpace = 0;

		// 0 ShadowSampler
		// 1 LinearSampler
		CD3DX12_STATIC_SAMPLER_DESC static_sampler[2];
		static_sampler[0] = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		static_sampler[1] = CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 0, 0, D3D12_COMPARISON_FUNC_NONE);

		D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
		RootSignatureDesc.NumParameters			= 6;
		RootSignatureDesc.pParameters			= &rootParams[0];
		RootSignatureDesc.Flags					= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT 
			| D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED // For bindless rendering
			| D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
		RootSignatureDesc.pStaticSamplers		= &static_sampler[0];
		RootSignatureDesc.NumStaticSamplers		= 2;

		result = renderer->CreateRootSignature(&RootSignatureDesc, &pRenderPrimitive->pRootSignature);
		assert(SUCCEEDED(result));
	}

	{
		ysn::ShaderCompileParameters vs_parameters;
		vs_parameters.shader_type = ysn::ShaderType::Vertex;
		vs_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"Shaders/BasePassVertex.hlsl");
		vs_parameters.defines = BuildAttributeDefines(pRenderPrimitive->RenderAttributes);

		const auto vs_shader_result = renderer->GetShaderStorage()->CompileShader(&vs_parameters);

		if (!vs_shader_result.has_value())
		{
			LogError << "Can't compile GLTF forward pipeline vs shader\n";
			return false;
		}

		ysn::ShaderCompileParameters ps_parameters;
		ps_parameters.shader_type = ysn::ShaderType::Pixel;
		ps_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"Shaders/BasePassPixelLighting.hlsl");
		ps_parameters.defines = vs_parameters.defines;

		const auto ps_shader_result = renderer->GetShaderStorage()->CompileShader(&ps_parameters);

		if (!ps_shader_result.has_value())
		{
			LogError << "Can't compile GLTF forward pipeline ps shader\n";
			return false;
		}

		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs = BuildInputElementDescs(pRenderPrimitive->RenderAttributes);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
		pipelineStateDesc.pRootSignature = pRenderPrimitive->pRootSignature.get();
		pipelineStateDesc.VS = { vs_shader_result.value()->GetBufferPointer(), vs_shader_result.value()->GetBufferSize() };
		pipelineStateDesc.PS = { ps_shader_result.value()->GetBufferPointer(), ps_shader_result.value()->GetBufferSize() };
		pipelineStateDesc.BlendState = pRenderPrimitive->pMaterial->blendDesc;
		pipelineStateDesc.SampleMask = UINT_MAX;
		pipelineStateDesc.RasterizerState = pRenderPrimitive->pMaterial->rasterizerDesc;
		pipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // TODO: provide
		pipelineStateDesc.DepthStencilState.DepthEnable = true;
		pipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		pipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		pipelineStateDesc.InputLayout = { inputElementDescs.data(), static_cast<UINT>(inputElementDescs.size()) };

		switch (pRenderPrimitive->primitiveTopology)
		{
			case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
				pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
				break;
			case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
			case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
				pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
				break;
			case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
			case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
				pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				break;
			default:
				assert(false);
		}

		pipelineStateDesc.NumRenderTargets = 1;
		pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT; // TODO: Provide it from outside
		pipelineStateDesc.SampleDesc = { 1, 0 };

		result = renderer->GetDevice()->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&pRenderPrimitive->pPipelineState));

		assert(SUCCEEDED(result));
	}

	return true;
}

bool ShadowPipeline(ysn::Primitive* pRenderPrimitive, std::shared_ptr<ysn::DxRenderer> renderer)
{
	HRESULT result = S_OK;

	{
		D3D12_ROOT_PARAMETER rootParams[2] = {
			{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 0, 0 }, D3D12_SHADER_VISIBILITY_VERTEX },
			{ D3D12_ROOT_PARAMETER_TYPE_CBV, { 1, 0 }, D3D12_SHADER_VISIBILITY_VERTEX }
		};

		// TEMP
		rootParams[0].Descriptor.RegisterSpace = 0;
		rootParams[1].Descriptor.RegisterSpace = 0;

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		rootSignatureDesc.NumParameters = _countof(rootParams);
		rootSignatureDesc.pParameters = &rootParams[0];
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		result = renderer->CreateRootSignature(&rootSignatureDesc, &pRenderPrimitive->pShadowRootSignature);

		assert(SUCCEEDED(result));
	}

	{
		ysn::ShaderCompileParameters vs_parameters;
		vs_parameters.shader_type = ysn::ShaderType::Vertex;
		vs_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"Shaders/BasePassVertex.hlsl");
		vs_parameters.defines = BuildAttributeDefines(pRenderPrimitive->RenderAttributes);
		vs_parameters.defines.emplace_back(L"SHADOW_PASS");

		const auto vs_shader_result = renderer->GetShaderStorage()->CompileShader(&vs_parameters);

		if (!vs_shader_result.has_value())
		{
			LogError << "Can't compile GLTF shadow pipeline vs shader\n";
			return false;
		}

		ysn::ShaderCompileParameters ps_parameters;
		ps_parameters.shader_type = ysn::ShaderType::Pixel;
		ps_parameters.shader_path = ysn::GetVirtualFilesystemPath(L"Shaders/BasePassPixelGray.hlsl");
		ps_parameters.defines = BuildAttributeDefines(pRenderPrimitive->RenderAttributes);

		const auto ps_shader_result = renderer->GetShaderStorage()->CompileShader(&ps_parameters);

		if (!ps_shader_result.has_value())
		{
			LogError << "Can't compile GLTF shadow pipeline ps shader\n";
			return false;
		}

		auto inputElementDescs = BuildInputElementDescs(pRenderPrimitive->RenderAttributes);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
		pipelineStateDesc.pRootSignature = pRenderPrimitive->pShadowRootSignature.get();
		pipelineStateDesc.VS = { vs_shader_result.value()->GetBufferPointer(), vs_shader_result.value()->GetBufferSize() };
		pipelineStateDesc.PS = { ps_shader_result.value()->GetBufferPointer(), ps_shader_result.value()->GetBufferSize() };
		pipelineStateDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		pipelineStateDesc.SampleMask = UINT_MAX;
		pipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT; // TODO: Why front?
		pipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // TODO: provide
		pipelineStateDesc.DepthStencilState.DepthEnable = true;
		pipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		pipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		pipelineStateDesc.InputLayout = { inputElementDescs.data(), static_cast<UINT>(inputElementDescs.size()) };

		switch (pRenderPrimitive->primitiveTopology)
		{
			case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
				pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
				break;
			case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
			case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
				pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
				break;
			case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
			case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
				pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				break;
			default:
				assert(false);
		}

		pipelineStateDesc.NumRenderTargets = 1;
		pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT; // TODO: Provide it from outside
		pipelineStateDesc.SampleDesc = { 1, 0 };

		result = renderer->GetDevice()->CreateGraphicsPipelineState(
			&pipelineStateDesc, IID_PPV_ARGS(&pRenderPrimitive->pShadowPipelineState));

		assert(SUCCEEDED(result));
	}

	return true;
}

void BuildPipelines(ysn::Primitive* pRenderPrimitive, std::shared_ptr<ysn::DxRenderer> renderer)
{
	// 1. Forward pass
	if (pRenderPrimitive->pMaterial != nullptr)
	{
		ForwardPipeline(pRenderPrimitive, renderer);
	}

	// 2. Forward pass no material
	// ForwardPipelineNoMaterial();

	// 2. Shadow pass
	ShadowPipeline(pRenderPrimitive, renderer);
}

void BuildPrimitiveMode(ysn::Primitive* pRenderPrimitive, const int GltfPrimitiveMode)
{
	switch (GltfPrimitiveMode)
	{
		case TINYGLTF_MODE_POINTS:
			pRenderPrimitive->primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
			break;
		case TINYGLTF_MODE_LINE:
			pRenderPrimitive->primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
			break;
		case TINYGLTF_MODE_LINE_STRIP:
			pRenderPrimitive->primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
			break;
		case TINYGLTF_MODE_TRIANGLES:
			pRenderPrimitive->primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			break;
		case TINYGLTF_MODE_TRIANGLE_STRIP:
			pRenderPrimitive->primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			break;
		default:
			assert(false);
	}
}

void BuildIndexBuffer(
	ysn::Primitive* pRenderPrimitive,
	const int GltfPrimitiveIndices,
	const tinygltf::Model* pGltfModel,
	const std::vector<wil::com_ptr<ID3D12Resource>>& pBuffers)
{
	if (GltfPrimitiveIndices >= 0)
	{
		const tinygltf::Accessor& GltfAccessor = pGltfModel->accessors[GltfPrimitiveIndices];
		const tinygltf::BufferView& GltfBufferView = pGltfModel->bufferViews[GltfAccessor.bufferView];
		// const tinygltf::Buffer& glTFBuffer = pModel->buffers[glTFBufferView.buffer];

		pRenderPrimitive->indexBufferView.BufferLocation = pBuffers[GltfBufferView.buffer]->GetGPUVirtualAddress() + GltfBufferView.byteOffset +
			GltfAccessor.byteOffset; // TODO: double byteoffset?
		pRenderPrimitive->indexBufferView.SizeInBytes = static_cast<UINT>(GltfBufferView.byteLength - GltfAccessor.byteOffset);

		switch (GltfAccessor.componentType)
		{
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				pRenderPrimitive->indexBufferView.Format = DXGI_FORMAT_R8_UINT;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				pRenderPrimitive->indexBufferView.Format = DXGI_FORMAT_R16_UINT;
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				pRenderPrimitive->indexBufferView.Format = DXGI_FORMAT_R32_UINT;
				break;
		}

		pRenderPrimitive->indexCount = static_cast<uint32_t>(GltfAccessor.count);

		pRenderPrimitive->index_count = pRenderPrimitive->indexCount;
		pRenderPrimitive->index_buffer = pBuffers[GltfBufferView.buffer];
		pRenderPrimitive->index_offset_in_bytes = GltfBufferView.byteOffset + GltfAccessor.byteOffset;
	}
	else
	{
		// Primitive don't have indices PERF
		// ysn::log::Warning();
	}
}

void BuildAccessorType(ysn::Attribute* pRenderAttribute, const int GltfAccessorType)
{
	switch (GltfAccessorType)
	{
		case TINYGLTF_TYPE_VEC2:
			pRenderAttribute->format = DXGI_FORMAT_R32G32_FLOAT;
			break;
		case TINYGLTF_TYPE_VEC3:
			pRenderAttribute->format = DXGI_FORMAT_R32G32B32_FLOAT;
			break;
		case TINYGLTF_TYPE_VEC4:
			pRenderAttribute->format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			break;
		default:;
	}
}

void BuildAttributesAccessors(
	ysn::Primitive* pRenderPrimitive,
	const tinygltf::Model* pGltfModel,
	const std::vector<wil::com_ptr<ID3D12Resource>>& pGltfBuffers,
	const std::map<std::string, int>& GltfPrimitiveAttributes)
{
	for (auto& [GltfAttributeName, GltfAccessorIndex] : GltfPrimitiveAttributes)
	{
		const tinygltf::Accessor& GltfAccessor = pGltfModel->accessors[GltfAccessorIndex];
		const tinygltf::BufferView& GltfBufferView = pGltfModel->bufferViews[GltfAccessor.bufferView];
		// const tinygltf::Buffer& glTFBuffer = pGltfModel->buffers[GltfBufferView.buffer];

		ysn::Attribute RenderAttribute = {};
		RenderAttribute.name = GltfAttributeName;

		BuildAccessorType(&RenderAttribute, GltfAccessor.type);

		RenderAttribute.vertexBufferView.BufferLocation = pGltfBuffers[GltfBufferView.buffer]->GetGPUVirtualAddress() +
			GltfBufferView.byteOffset +
			GltfAccessor.byteOffset; // TODO: what is this offsets?

		RenderAttribute.vertexBufferView.SizeInBytes = static_cast<UINT>(GltfBufferView.byteLength - GltfAccessor.byteOffset);
		RenderAttribute.vertexBufferView.StrideInBytes = GltfAccessor.ByteStride(GltfBufferView);

		if (GltfAttributeName == "POSITION")
		{
			pRenderPrimitive->vertexCount = static_cast<uint32_t>(GltfAccessor.count);
			pRenderPrimitive->vertex_count = pRenderPrimitive->vertexCount;
			pRenderPrimitive->vertex_buffer = pGltfBuffers[GltfBufferView.buffer];
			pRenderPrimitive->vertex_stride = RenderAttribute.vertexBufferView.StrideInBytes;
			pRenderPrimitive->vertex_offset_in_bytes = GltfBufferView.byteOffset + GltfAccessor.byteOffset;
		}

		pRenderPrimitive->RenderAttributes.emplace_back(RenderAttribute);
	}
}

void BuildMeshes(ysn::ModelRenderContext* pModelRenderContext, tinygltf::Model* pGltfModel, std::shared_ptr<ysn::DxRenderer> renderer)
{
	for (tinygltf::Mesh& GltfMesh : pGltfModel->meshes)
	{
		ysn::Mesh RenderMesh = {};
		RenderMesh.name = GltfMesh.name;

		for (tinygltf::Primitive& GltfPrimitive : GltfMesh.primitives)
		{
			ysn::Primitive RenderPrimitive = {};

			BuildAttributesAccessors(&RenderPrimitive, pGltfModel, pModelRenderContext->pBuffers, GltfPrimitive.attributes);
			BuildIndexBuffer(&RenderPrimitive, GltfPrimitive.indices, pGltfModel, pModelRenderContext->pBuffers);
			BuildPrimitiveMode(&RenderPrimitive, GltfPrimitive.mode);

			if (GltfPrimitive.material >= 0)
			{
				RenderPrimitive.pMaterial = &pModelRenderContext->Materials[GltfPrimitive.material]; // TODO: Move out
			}
			BuildPipelines(&RenderPrimitive, renderer);

			RenderMesh.primitives.push_back(RenderPrimitive);
		}

		pModelRenderContext->Meshes.push_back(RenderMesh);
	}
}

void BuildNodes(ysn::ModelRenderContext* ModelRenderContext, wil::com_ptr<ID3D12Device5> pRenderDevice, tinygltf::Model* pModel)
{
	HRESULT hr = S_OK;

	for (tinygltf::Node& glTFNode : pModel->nodes)
	{
		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = ysn::AlignPow2(sizeof(ysn::PBRMetallicRoughnessShader), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc = { 1, 0 };
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		wil::com_ptr<ID3D12Resource> pBuffer;
		hr = pRenderDevice->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&pBuffer));
		assert(SUCCEEDED(hr));

		void* pData;
		hr = pBuffer->Map(0, nullptr, &pData);
		assert(SUCCEEDED(hr));

		if (glTFNode.matrix.empty())
		{
			XMStoreFloat4x4(static_cast<DirectX::XMFLOAT4X4*>(pData), DirectX::XMMatrixIdentity());
		}
		else
		{
			float* pElement = static_cast<float*>(pData);
		
			for (double value : glTFNode.matrix)
			{
				*pElement = static_cast<float>(value);
				++pElement;
			}
		}

		ModelRenderContext->pNodeBuffers.push_back(pBuffer);
	}
}

void DrawNode(
	ysn::ModelRenderContext* ModelRenderContext,
	std::shared_ptr<ysn::DxRenderer> p_renderer,
	tinygltf::Model* pModel,
	wil::com_ptr<ID3D12GraphicsCommandList> pCommandList,
	wil::com_ptr<ID3D12Resource> pCameraBuffer,
	wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer,
	uint64_t nodeIndex,
	ysn::PrimitivePipeline PrimitivePipeline,
	ysn::ShadowMapBuffer* p_shadow_map_buffer)
{
	const auto& glTFNode = pModel->nodes[nodeIndex];

	if (glTFNode.mesh >= 0)
	{
		const auto& mesh = ModelRenderContext->Meshes[glTFNode.mesh];

		for (const ysn::Primitive& primitive : mesh.primitives)
		{
			pCommandList->IASetPrimitiveTopology(primitive.primitiveTopology);

			for (auto i = 0; i != primitive.RenderAttributes.size(); ++i)
			{
				pCommandList->IASetVertexBuffers(i, 1, &primitive.RenderAttributes[i].vertexBufferView);
			}

			ID3D12DescriptorHeap* pDescriptorHeaps[] = {
				p_renderer->GetCbvSrvUavDescriptorHeap()->GetHeapPtr(),
			};
			pCommandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

			switch (PrimitivePipeline)
			{
				case ysn::PrimitivePipeline::ForwardPbr:
				{
					pCommandList->SetGraphicsRootSignature(primitive.pRootSignature.get());
					pCommandList->SetPipelineState(primitive.pPipelineState.get());
					pCommandList->SetGraphicsRootConstantBufferView(2, primitive.pMaterial->pBuffer->GetGPUVirtualAddress());
					pCommandList->SetGraphicsRootConstantBufferView(3, scene_parameters_gpu_buffer->GetGPUVirtualAddress());
					pCommandList->SetGraphicsRootDescriptorTable(4, primitive.pMaterial->srv_handle.gpu);
					pCommandList->SetGraphicsRootDescriptorTable(5, p_shadow_map_buffer->srv_handle.gpu);
					break;
				}
				case ysn::PrimitivePipeline::Shadow:
					pCommandList->SetGraphicsRootSignature(primitive.pShadowRootSignature.get());
					pCommandList->SetPipelineState(primitive.pShadowPipelineState.get());
					break;
				case ysn::PrimitivePipeline::ForwardNoMaterial:
					break;
			}

			pCommandList->SetGraphicsRootConstantBufferView(0, pCameraBuffer->GetGPUVirtualAddress());
			pCommandList->SetGraphicsRootConstantBufferView(1, ModelRenderContext->pNodeBuffers[nodeIndex]->GetGPUVirtualAddress());


			if (primitive.indexCount)
			{
				pCommandList->IASetIndexBuffer(&primitive.indexBufferView);
				pCommandList->DrawIndexedInstanced(primitive.indexCount, 1, 0, 0, 0);
			}
			else
			{
				pCommandList->DrawInstanced(primitive.vertexCount, 1, 0, 0);
			}
		}
	}

	for (auto childNodeIndex : glTFNode.children)
	{
		DrawNode(ModelRenderContext, p_renderer, pModel, pCommandList, pCameraBuffer, scene_parameters_gpu_buffer, childNodeIndex, PrimitivePipeline, p_shadow_map_buffer);
	}
}

void ysn::RenderGLTF(
	ysn::ModelRenderContext* ModelRenderContext,
	std::shared_ptr<ysn::DxRenderer> p_renderer,
	tinygltf::Model* pModel,
	wil::com_ptr<ID3D12GraphicsCommandList> pCommandList,
	wil::com_ptr<ID3D12Resource> pCameraBuffer,
	wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer,
	ysn::PrimitivePipeline PrimitivePipeline,
	ysn::ShadowMapBuffer* p_shadow_map_descriptor)
{
	auto& scene = pModel->scenes[pModel->defaultScene];

	for (auto nodeIndex : scene.nodes)
	{
		DrawNode(ModelRenderContext, p_renderer, pModel, pCommandList, pCameraBuffer, scene_parameters_gpu_buffer, nodeIndex, PrimitivePipeline, p_shadow_map_descriptor);
	}
}

DXGI_FORMAT MakeSRGB(DXGI_FORMAT format)
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
// 
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

void BuildImages(
	ysn::ModelRenderContext* ModelRenderContext,
	std::shared_ptr<ysn::DxRenderer> p_renderer,
	wil::com_ptr<ID3D12GraphicsCommandList> pCopyCommandList,
	tinygltf::Model* pModel,
	)
{
	HRESULT hr = S_OK;

	for (tinygltf::Image& image : pModel->images)
	{
		// TODO: madness
		const uint32_t current_texture_index = static_cast<uint32_t>(ModelRenderContext->pTextures.size());
		const bool is_srgb = ModelRenderContext->srgb_textures.contains(current_texture_index);

		const uint32_t num_mips = ComputeNumMips(image.width, image.height);

		wil::com_ptr<ID3D12Resource> pDstTexture;
		{
			D3D12_HEAP_PROPERTIES heapProperties = {};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			// TODO: sRGB albedo textures?
			// TODO: HDR textures?
			D3D12_RESOURCE_DESC resourceDesc = {};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = image.width;
			resourceDesc.Height = image.height;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = num_mips;
			resourceDesc.Format = is_srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
			resourceDesc.SampleDesc = { 1, 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			hr = p_renderer->GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pDstTexture));
			assert(SUCCEEDED(hr));

		#ifndef YSN_RELEASE
			std::wstring name(image.name.begin(), image.name.end());
			pDstTexture->SetName(name.c_str());
		#endif

			ysn::Texture new_texture;
			new_texture.gpuTexture = pDstTexture;
			new_texture.is_srgb = is_srgb;
			new_texture.num_mips = num_mips;
			new_texture.width = image.width;
			new_texture.height = image.height;

			ModelRenderContext->pTextures.push_back(new_texture);
		}

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
		UINT rowCount;
		UINT64 rowSize;
		UINT64 size;

		const auto dstTextureDesc = pDstTexture->GetDesc();
		// TODO: What is that?
		p_renderer->GetDevice()->GetCopyableFootprints(&dstTextureDesc, 0, 1, 0, &footprint, &rowCount, &rowSize, &size);

		wil::com_ptr<ID3D12Resource> pSrcBuffer;
		{
			D3D12_HEAP_PROPERTIES heapProperties = {};
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			D3D12_RESOURCE_DESC resourceDesc = {};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = size;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc = { 1, 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			hr = p_renderer->GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&pSrcBuffer));

			assert(SUCCEEDED(hr));
			pStagingResources->push_back(pSrcBuffer);

			void* pData;
			hr = pSrcBuffer->Map(0, nullptr, &pData);
			assert(SUCCEEDED(hr));

			for (UINT i = 0; i != rowCount; ++i)
			{
				memcpy(
					static_cast<uint8_t*>(pData) + rowSize * i,
					&image.image[0] + image.width * image.component * i,
					image.width * image.component);
			}
		}

		D3D12_TEXTURE_COPY_LOCATION dstCopyLocation = {};
		dstCopyLocation.pResource = pDstTexture.get();
		dstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstCopyLocation.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION srcCopyLocation = {};
		srcCopyLocation.pResource = pSrcBuffer.get();
		srcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		srcCopyLocation.PlacedFootprint = footprint;

		pCopyCommandList->CopyTextureRegion(&dstCopyLocation, 0, 0, 0, &srcCopyLocation, nullptr);
	}
}

*/
//
//void ReadModel(RenderScene& render_scene, const tinygltf::Model& gltf_model)
//{
//	//Application::Get().GetRenderer();
//	//wil::com_ptr<ID3D12GraphicsCommandList4> command_list = command_queue->GetCommandList();
//
//	std::vector<wil::com_ptr<ID3D12Resource>> staging_resources;
//	staging_resources.reserve(256);
//
//	//BuildBuffers(ModelRenderContext, p_renderer, pCopyCommandList, pModel, &ModelRenderContext->stagingResources);
//	//FindAllSrgbTextures(ModelRenderContext, pModel);
//	//BuildImages(ModelRenderContext, p_renderer, pCopyCommandList, pModel, &ModelRenderContext->stagingResources);
//	//BuildSamplerDescs(ModelRenderContext, pModel);
//	//BuildMaterials(ModelRenderContext, p_renderer, pCopyCommandList, pModel);
//	//BuildMeshes(ModelRenderContext, pModel, p_renderer);
//	//BuildNodes(ModelRenderContext, p_renderer->GetDevice(), pModel);
//}

namespace ysn
{
	bool LoadGltfFromFile(RenderScene& render_scene, const std::wstring& load_path, const LoadingParameters& loading_parameters)
	{
		tinygltf::Model gltf_model;
		tinygltf::TinyGLTF gltf_loader;

		std::string error_str;
		std::string warning_str;
		const std::string load_path_str = ysn::WStringToString(load_path);

		const bool result = gltf_loader.LoadASCIIFromFile(&gltf_model, &error_str, &warning_str, load_path_str.c_str());

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

		//ReadModel(render_scene, &model);

		return true;
	}
}

