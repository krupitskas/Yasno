#include "Pso.hpp"

#include <System/Hash.hpp>

namespace ysn
{
	GraphicsPsoDesc::GraphicsPsoDesc(std::string name)
	{
		m_name = name;
	}

	PsoId GraphicsPsoDesc::GenerateId() const
	{
		auto pso_desc_copy = m_pso_desc;

		pso_desc_copy.InputLayout.pInputElementDescs = nullptr;

		PsoId pso_id = HashState(&pso_desc_copy);
		pso_id = HashState(m_input_layout.get(), m_pso_desc.InputLayout.NumElements, pso_id);

		//m_pso_desc.InputLayout.pInputElementDescs = m_input_layout.get();

		return pso_id;
	}

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC& GraphicsPsoDesc::GetDesc() const
	{
		return m_pso_desc;
	}

	std::string GraphicsPsoDesc::GetName() const
	{
		return m_name;
	}

	std::wstring GraphicsPsoDesc::GetNameWString() const
	{
		std::wstring name(m_name.begin(), m_name.end());
		return name;
	}

	ID3D12RootSignature* GraphicsPsoDesc::GetRootSignature() const
	{
		return m_pso_desc.pRootSignature;
	}

	void GraphicsPsoDesc::SetRootSignature(ID3D12RootSignature* root_signature)
	{
		m_pso_desc.pRootSignature = root_signature;
		YSN_ASSERT(m_pso_desc.pRootSignature != nullptr);
	}

	void GraphicsPsoDesc::SetBlendState(const D3D12_BLEND_DESC& blend_desc)
	{
		m_pso_desc.BlendState = blend_desc;
	}

	void GraphicsPsoDesc::SetRasterizerState(const D3D12_RASTERIZER_DESC& rasterizer_desc)
	{
		m_pso_desc.RasterizerState = rasterizer_desc;
	}

	void GraphicsPsoDesc::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& depth_stencil_desc)
	{
		m_pso_desc.DepthStencilState = depth_stencil_desc;
	}

	void GraphicsPsoDesc::SetSampleMask(UINT sample_mask)
	{
		m_pso_desc.SampleMask = sample_mask;
	}

	void GraphicsPsoDesc::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type)
	{
		YSN_ASSERT_MSG(topology_type != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED, "Can't draw with undefined topology");
		m_pso_desc.PrimitiveTopologyType = topology_type;
	}

	void GraphicsPsoDesc::SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ib_props)
	{
		m_pso_desc.IBStripCutValue = ib_props;
	}

	void GraphicsPsoDesc::SetDepthTargetFormat(DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
	{
		SetRenderTargetFormats(0, nullptr, DSVFormat, MsaaCount, MsaaQuality);
	}

	void GraphicsPsoDesc::SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
	{
		SetRenderTargetFormats(1, &RTVFormat, DSVFormat, MsaaCount, MsaaQuality);
	}

	void GraphicsPsoDesc::SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
	{
		YSN_ASSERT_MSG(NumRTVs == 0 || RTVFormats != nullptr, "Null format array conflicts with non-zero length");
		for (UINT i = 0; i < NumRTVs; ++i)
		{
			YSN_ASSERT(RTVFormats[i] != DXGI_FORMAT_UNKNOWN);
			m_pso_desc.RTVFormats[i] = RTVFormats[i];
		}
		//for (UINT i = NumRTVs; i < m_pso_desc.NumRenderTargets; ++i)
		//	m_pso_desc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
		m_pso_desc.NumRenderTargets = NumRTVs;
		m_pso_desc.DSVFormat = DSVFormat;
		m_pso_desc.SampleDesc.Count = MsaaCount;
		m_pso_desc.SampleDesc.Quality = MsaaQuality;
	}

	void GraphicsPsoDesc::SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs)
	{
		m_pso_desc.InputLayout.NumElements = NumElements;

		if (NumElements > 0)
		{
			D3D12_INPUT_ELEMENT_DESC* new_elements = (D3D12_INPUT_ELEMENT_DESC*)malloc(sizeof(D3D12_INPUT_ELEMENT_DESC) * NumElements);
			memcpy(new_elements, pInputElementDescs, NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
			m_input_layout.reset((D3D12_INPUT_ELEMENT_DESC*)new_elements);

			m_pso_desc.InputLayout.pInputElementDescs = m_input_layout.get();
		}
		else
		{
			m_input_layout = nullptr;
		}
	}

	void GraphicsPsoDesc::SetVertexShader(const void* Binary, size_t Size)
	{
		m_pso_desc.VS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
	}

	void GraphicsPsoDesc::SetPixelShader(const void* Binary, size_t Size)
	{
		m_pso_desc.PS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
	}

	void GraphicsPsoDesc::SetGeometryShader(const void* Binary, size_t Size)
	{
		m_pso_desc.GS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
	}

	void GraphicsPsoDesc::SetHullShader(const void* Binary, size_t Size)
	{
		m_pso_desc.HS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
	}

	void GraphicsPsoDesc::SetDomainShader(const void* Binary, size_t Size)
	{
		m_pso_desc.DS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
	}

	void GraphicsPsoDesc::SetVertexShader(const D3D12_SHADER_BYTECODE& Binary)
	{
		m_pso_desc.VS = Binary;
	}

	void GraphicsPsoDesc::SetPixelShader(const D3D12_SHADER_BYTECODE& Binary)
	{
		m_pso_desc.PS = Binary;
	}

	void GraphicsPsoDesc::SetGeometryShader(const D3D12_SHADER_BYTECODE& Binary)
	{
		m_pso_desc.GS = Binary;
	}

	void GraphicsPsoDesc::SetHullShader(const D3D12_SHADER_BYTECODE& Binary)
	{
		m_pso_desc.HS = Binary;
	}

	void GraphicsPsoDesc::SetDomainShader(const D3D12_SHADER_BYTECODE& Binary)
	{
		m_pso_desc.DS = Binary;
	}

	std::optional<PsoId> PsoStorage::CreateGraphicsPso(wil::com_ptr<ID3D12Device5> device, const GraphicsPsoDesc& pso_desc)
	{
		const PsoId pso_id = pso_desc.GenerateId();

		ID3D12PipelineState** pso_ref = nullptr;

		bool should_create_new = !m_graphics_pso_pool.contains(pso_id);

		if (should_create_new)
		{
			//YSN_ASSERT(pso_ref->DepthStencilState.DepthEnable != (m_pso_desc.DSVFormat == DXGI_FORMAT_UNKNOWN));

			Pso new_pso;

			const auto res = device->CreateGraphicsPipelineState(&pso_desc.GetDesc(), IID_PPV_ARGS(&new_pso.pso));

			if (res != S_OK)
			{
				LogFatal << "Can't compile pso '" << pso_desc.GetName() << "' " << pso_id << " \n";
				return std::nullopt;
			}

			LogInfo << "Compiled new pso " << pso_desc.GetName() << " " << pso_id << " \n";

			new_pso.pso_id = pso_id;
			new_pso.root_signature.attach(pso_desc.GetRootSignature());


		#ifndef YSN_RELEASE
			new_pso.pso->SetName(pso_desc.GetNameWString().c_str());
		#endif

			m_graphics_pso_pool[pso_id] = new_pso;
		}
		else
		{
			LogInfo << "Cache hit for pso: " << pso_desc.GetName() << " " << pso_id << " \n";
		}

		return pso_id;
	}

	std::optional<Pso> PsoStorage::GetPso(PsoId pso_id)
	{
		const bool is_found = m_graphics_pso_pool.contains(pso_id);

		if (is_found)
		{
			return m_graphics_pso_pool[pso_id];
		}
		else
		{
			return std::nullopt;
		}
	}
}