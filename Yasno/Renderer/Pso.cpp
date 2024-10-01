#include "Pso.hpp"

namespace ysn
{
	GraphicsPso::GraphicsPso()
	{
		type = PsoType::Graphics;
	}

	ComputePso::ComputePso()
	{
		type = PsoType::Compute;
	}

	inline void ComputePso::SetComputeShader(const void* binary, size_t size)
	{
		m_pso_desc.CS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(binary), size);
	}

	inline void ComputePso::SetComputeShader(const D3D12_SHADER_BYTECODE& binary)
	{
		m_pso_desc.CS = binary;
	}

	/*
	void GraphicsPSO::Finalize()
	{
		// Make sure the root signature is finalized first
		m_PSODesc.pRootSignature = m_RootSignature->GetSignature();
		ASSERT(m_PSODesc.pRootSignature != nullptr);

		m_PSODesc.InputLayout.pInputElementDescs = nullptr;
		size_t HashCode = Utility::HashState(&m_PSODesc);
		HashCode = Utility::HashState(m_InputLayouts.get(), m_PSODesc.InputLayout.NumElements, HashCode);
		m_PSODesc.InputLayout.pInputElementDescs = m_InputLayouts.get();

		ID3D12PipelineState** PSORef = nullptr;
		bool firstCompile = false;
		{
			static mutex s_HashMapMutex;
			lock_guard<mutex> CS(s_HashMapMutex);
			auto iter = s_GraphicsPSOHashMap.find(HashCode);

			// Reserve space so the next inquiry will find that someone got here first.
			if (iter == s_GraphicsPSOHashMap.end())
			{
				firstCompile = true;
				PSORef = s_GraphicsPSOHashMap[HashCode].GetAddressOf();
			}
			else
				PSORef = iter->second.GetAddressOf();
		}

		if (firstCompile)
		{
			ASSERT(m_PSODesc.DepthStencilState.DepthEnable != (m_PSODesc.DSVFormat == DXGI_FORMAT_UNKNOWN));
			ASSERT_SUCCEEDED( g_Device->CreateGraphicsPipelineState(&m_PSODesc, MY_IID_PPV_ARGS(&m_PSO)) );
			s_GraphicsPSOHashMap[HashCode].Attach(m_PSO);
			m_PSO->SetName(m_Name);
		}
		else
		{
			while (*PSORef == nullptr)
				this_thread::yield();
			m_PSO = *PSORef;
		}
	}


	void ComputePSO::Finalize()
	{
		// Make sure the root signature is finalized first
		m_PSODesc.pRootSignature = m_RootSignature->GetSignature();
		ASSERT(m_PSODesc.pRootSignature != nullptr);

		size_t HashCode = Utility::HashState(&m_PSODesc);

		ID3D12PipelineState** PSORef = nullptr;
		bool firstCompile = false;
		{
			static mutex s_HashMapMutex;
			lock_guard<mutex> CS(s_HashMapMutex);
			auto iter = s_ComputePSOHashMap.find(HashCode);

			// Reserve space so the next inquiry will find that someone got here first.
			if (iter == s_ComputePSOHashMap.end())
			{
				firstCompile = true;
				PSORef = s_ComputePSOHashMap[HashCode].GetAddressOf();
			}
			else
				PSORef = iter->second.GetAddressOf();
		}

		if (firstCompile)
		{
			ASSERT_SUCCEEDED( g_Device->CreateComputePipelineState(&m_PSODesc, MY_IID_PPV_ARGS(&m_PSO)) );
			s_ComputePSOHashMap[HashCode].Attach(m_PSO);
			m_PSO->SetName(m_Name);
		}
		else
		{
			while (*PSORef == nullptr)
				this_thread::yield();
			m_PSO = *PSORef;
		}
	}
	*/

	void GraphicsPso::Build()
	{
	}

	void ComputePso::Build()
	{
	}

	void GraphicsPso::SetBlendState(const D3D12_BLEND_DESC& blend_desc)
	{
		m_pso_desc.BlendState = blend_desc;
	}

	void GraphicsPso::SetRasterizerState(const D3D12_RASTERIZER_DESC& rasterizer_desc)
	{
		m_pso_desc.RasterizerState = rasterizer_desc;
	}

	void GraphicsPso::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& depth_stencil_desc)
	{
		m_pso_desc.DepthStencilState = depth_stencil_desc;
	}

	void GraphicsPso::SetSampleMask(UINT sample_mask)
	{
		m_pso_desc.SampleMask = sample_mask;
	}

	void GraphicsPso::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type)
	{
		YSN_ASSERT_MSG(topology_type != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED, "Can't draw with undefined topology");
		m_pso_desc.PrimitiveTopologyType = topology_type;
	}

	void GraphicsPso::SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE ib_props)
	{
		m_pso_desc.IBStripCutValue = ib_props;
	}

	void GraphicsPso::SetDepthTargetFormat(DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
	{
		SetRenderTargetFormats(0, nullptr, DSVFormat, MsaaCount, MsaaQuality);
	}

	void GraphicsPso::SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
	{
		SetRenderTargetFormats(1, &RTVFormat, DSVFormat, MsaaCount, MsaaQuality);
	}

	void GraphicsPso::SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
	{
		YSN_ASSERT_MSG(NumRTVs == 0 || RTVFormats != nullptr, "Null format array conflicts with non-zero length");
		for (UINT i = 0; i < NumRTVs; ++i)
		{
			YSN_ASSERT(RTVFormats[i] != DXGI_FORMAT_UNKNOWN);
			m_pso_desc.RTVFormats[i] = RTVFormats[i];
		}
		for (UINT i = NumRTVs; i < m_pso_desc.NumRenderTargets; ++i)
			m_pso_desc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
		m_pso_desc.NumRenderTargets = NumRTVs;
		m_pso_desc.DSVFormat = DSVFormat;
		m_pso_desc.SampleDesc.Count = MsaaCount;
		m_pso_desc.SampleDesc.Quality = MsaaQuality;
	}

	void GraphicsPso::SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* pInputElementDescs)
	{
		m_pso_desc.InputLayout.NumElements = NumElements;

		if (NumElements > 0)
		{
			D3D12_INPUT_ELEMENT_DESC* NewElements = (D3D12_INPUT_ELEMENT_DESC*)malloc(sizeof(D3D12_INPUT_ELEMENT_DESC) * NumElements);
			memcpy(NewElements, pInputElementDescs, NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
			m_input_layout.reset((const D3D12_INPUT_ELEMENT_DESC*)NewElements);
		}
		else
			m_input_layout = nullptr;
	}


	// These const_casts shouldn't be necessary, but we need to fix the API to accept "const void* pShaderBytecode"

	inline void GraphicsPso::SetVertexShader(const void* Binary, size_t Size)
	{
		m_pso_desc.VS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
	}

	inline void GraphicsPso::SetPixelShader(const void* Binary, size_t Size)
	{
		m_pso_desc.PS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
	}

	inline void GraphicsPso::SetGeometryShader(const void* Binary, size_t Size)
	{
		m_pso_desc.GS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
	}

	inline void GraphicsPso::SetHullShader(const void* Binary, size_t Size)
	{
		m_pso_desc.HS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
	}

	inline void GraphicsPso::SetDomainShader(const void* Binary, size_t Size)
	{
		m_pso_desc.DS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size);
	}

	inline void GraphicsPso::SetVertexShader(const D3D12_SHADER_BYTECODE& Binary)
	{
		m_pso_desc.VS = Binary;
	}

	inline void GraphicsPso::SetPixelShader(const D3D12_SHADER_BYTECODE& Binary)
	{
		m_pso_desc.PS = Binary;
	}

	inline void GraphicsPso::SetGeometryShader(const D3D12_SHADER_BYTECODE& Binary)
	{
		m_pso_desc.GS = Binary;
	}

	inline void GraphicsPso::SetHullShader(const D3D12_SHADER_BYTECODE& Binary)
	{
		m_pso_desc.HS = Binary;
	}

	inline void GraphicsPso::SetDomainShader(const D3D12_SHADER_BYTECODE& Binary)
	{
		m_pso_desc.DS = Binary;
	}

}