#pragma once

#include <string>
#include <unordered_map>

#include <d3dx12.h>
#include <wil/com.h>

namespace ysn
{
	using PsoId = size_t;

	enum class PsoType
	{
		None,
		Graphics,
		Compute
	};

	// TODO: Split into Pso and PsoDescription

	struct Pso
	{
		Pso() = default;

		void SetName(std::string name)
		{
			m_name = name;
		}

		wil::com_ptr<ID3D12RootSignature> m_root_signature = nullptr;
		wil::com_ptr<ID3D12PipelineState> m_pso = nullptr;
	protected:
		PsoId pso_id = 0;
		PsoType type = PsoType::None;
		std::string m_name = "Neyasnoe PSO";
	};

	struct GraphicsPso : public Pso
	{
		GraphicsPso();

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

		std::optional<PsoId> Build(wil::com_ptr<ID3D12Device5> device, std::unordered_map<PsoId, GraphicsPso>& pso_pool);
	protected:

		D3D12_GRAPHICS_PIPELINE_STATE_DESC m_pso_desc = {};
		std::shared_ptr<const D3D12_INPUT_ELEMENT_DESC> m_input_layout;
	};

	struct ComputePso : public Pso
	{
		ComputePso();

		void SetComputeShader(const void* binary, size_t size);
		void SetComputeShader(const D3D12_SHADER_BYTECODE& binary);
		bool Build(std::unordered_map<PsoId, GraphicsPso>& pso_pool);
	protected:
		D3D12_COMPUTE_PIPELINE_STATE_DESC m_pso_desc = {};
	};
}