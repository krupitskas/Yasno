#pragma once

#include <unordered_map>
#include <memory>
#include <unordered_set>

#include <d3d12.h>
#include <d3dx12.h>
#include <wil/com.h>
#include <tiny_gltf.h>
#include <DirectXMath.h>

#include <Renderer/DescriptorHeap.hpp>
#include <Renderer/GpuTexture.hpp>
#include <Graphics/RenderScene.hpp>

namespace ysn
{
	struct ShadowMapBuffer;
	class DxRenderer;

	struct Material
	{
		std::string name;

		D3D12_BLEND_DESC blendDesc;
		D3D12_RASTERIZER_DESC rasterizerDesc;
		void* pBufferData;

		DescriptorHandle srv_handle;
		DescriptorHandle sampler_handle;

		wil::com_ptr<ID3D12Resource> pBuffer;
	};

	struct Attribute
	{
		std::string name;

		DXGI_FORMAT format;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	};

	enum class PrimitivePipeline
	{
		ForwardPbr,
		ForwardNoMaterial,
		Shadow
	};

	struct Primitive
	{
		std::vector<Attribute> RenderAttributes;

		D3D12_PRIMITIVE_TOPOLOGY primitiveTopology;

		uint32_t vertexCount;

		D3D12_INDEX_BUFFER_VIEW indexBufferView;
		uint32_t indexCount;

		Material* pMaterial;

		wil::com_ptr<ID3D12RootSignature> pRootSignature;
		wil::com_ptr<ID3D12PipelineState> pPipelineState;

		wil::com_ptr<ID3D12RootSignature> pShadowRootSignature;
		wil::com_ptr<ID3D12PipelineState> pShadowPipelineState;

		// TODO: This is lazy way to keep eye on this info for RTX BLAS generation, clean that up later
		wil::com_ptr<ID3D12Resource> vertex_buffer;
		uint64_t vertex_offset_in_bytes;
		uint32_t vertex_count;
		uint32_t vertex_stride;

		wil::com_ptr<ID3D12Resource> index_buffer;
		uint64_t index_offset_in_bytes;
		uint32_t index_count;
		// ...
	};

	// TODO: Rename to RenderMesh
	struct Mesh
	{
		std::string name;
		std::vector<Primitive> primitives;
	};

	//struct Node
	//{
	//	DirectX::XMFLOAT4X4 M;
	//};

	//struct ModelRenderContext
	//{
	//	std::vector<wil::com_ptr<ID3D12Resource>> pBuffers;
	//	std::vector<Texture> pTextures;
	//	std::unordered_set<uint32_t> srgb_textures;
	//	std::vector<D3D12_SAMPLER_DESC> SamplerDescs;
	//	std::vector<Material> Materials;
	//	std::vector<Mesh> Meshes;
	//	std::vector<wil::com_ptr<ID3D12Resource>> pNodeBuffers;
	//	std::vector<wil::com_ptr<ID3D12Resource>> stagingResources; // TODO: make it temp?

	//	tinygltf::Model gltfModel;
	//};

	struct LoadingParameters
	{
		DirectX::XMMATRIX model_modifier = DirectX::XMMatrixIdentity(); // Applies matrix modifier for nodes and RTX BVH generation
	};

	bool LoadGltfFromFile(RenderScene& render_scene, const std::wstring& path, const LoadingParameters& loading_parameters);

	//void RenderGLTF(ysn::ModelRenderContext* pGLTFDrawContext,
	//				std::shared_ptr<ysn::DxRenderer> p_renderer,
	//				tinygltf::Model* pModel,
	//				wil::com_ptr<ID3D12GraphicsCommandList> pCommandList,
	//				wil::com_ptr<ID3D12Resource> pCameraBuffer,
	//				wil::com_ptr<ID3D12Resource> scene_parameters_gpu_buffer,
	//				ysn::PrimitivePipeline PrimitivePipeline,
	//				ysn::ShadowMapBuffer* p_shadow_map_descriptor);
}
