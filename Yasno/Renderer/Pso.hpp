#pragma once

#include <string>
#include <unordered_map>

#include <d3dx12.h>
#include <wil/com.h>

namespace ysn
{
	using PsoId = size_t;

	struct ComputePsoDesc
	{
		void SetComputeShader(const void* binary, size_t size);
		void SetComputeShader(const D3D12_SHADER_BYTECODE& binary);
	private:
		D3D12_COMPUTE_PIPELINE_STATE_DESC m_pso_desc = {};
	};

	struct GraphicsPsoDesc
	{
		GraphicsPsoDesc(std::string name);

		PsoId GenerateId() const;
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC& GetDesc() const;
		std::string GetName() const;
		std::wstring GetNameWString() const;
		ID3D12RootSignature* GetRootSignature() const;

		void SetRootSignature(ID3D12RootSignature* root_signature);

		void SetBlendState(const D3D12_BLEND_DESC& blend_desc);
		void SetRasterizerState(const D3D12_RASTERIZER_DESC& rasterizer_desc);
		void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& depth_stencil_desc);
		void SetSampleMask(UINT sample_mask);
		void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type);
		void SetDepthTargetFormat(DXGI_FORMAT dsv_format, UINT msaa_count = 1, UINT msaa_quality = 0);
		void SetRenderTargetFormat(DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format, UINT msaa_count = 1, UINT msaa_quality = 0);
		void SetRenderTargetFormats(UINT num_rtv, const DXGI_FORMAT* rtv_formats, DXGI_FORMAT dsv_format, UINT msaa_count = 1, UINT msaa_quality = 0);
		void SetInputLayout(UINT num_elements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs);
		void SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ib_props);

		// These const_casts shouldn't be necessary, but we need to fix the API to accept "const void* pShaderBytecode"
		void SetVertexShader(const void* binary, size_t size);
		void SetPixelShader(const void* binary, size_t size);
		void SetGeometryShader(const void* binary, size_t size);
		void SetHullShader(const void* binary, size_t size);
		void SetDomainShader(const void* binary, size_t size);

		void SetVertexShader(const D3D12_SHADER_BYTECODE& binary);
		void SetPixelShader(const D3D12_SHADER_BYTECODE& binary);
		void SetGeometryShader(const D3D12_SHADER_BYTECODE& binary);
		void SetHullShader(const D3D12_SHADER_BYTECODE& binary);
		void SetDomainShader(const D3D12_SHADER_BYTECODE& binary);

		//std::optional<PsoId> Build(wil::com_ptr<ID3D12Device5> device, std::unordered_map<PsoId, GraphicsPso>& pso_pool);

	private:
		std::string m_name = "Unnamed PSO";

		//wil::com_ptr<ID3D12RootSignature> m_root_signature = nullptr;
		std::shared_ptr<D3D12_INPUT_ELEMENT_DESC> m_input_layout;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC m_pso_desc = {};
	};

	struct Pso
	{
		PsoId pso_id = -1;
		wil::com_ptr<ID3D12RootSignature> root_signature = nullptr;
		wil::com_ptr<ID3D12PipelineState> pso = nullptr;
	};

	struct PsoStorage
	{
		std::optional<PsoId> CreateGraphicsPso(wil::com_ptr<ID3D12Device5> device, const GraphicsPsoDesc& pso_desc);
		//std::optional<PsoId> CreateComputePso();

		std::optional<Pso> GetPso(PsoId pso_id);
		//std::optional<GraphicsPso> GetGraphicsPso();

		//void DeletePso();

		std::unordered_map<PsoId, Pso> m_graphics_pso_pool;
		//std::unordered_map<PsoId, ComputePso> m_compute_pso_pool;
	};
}