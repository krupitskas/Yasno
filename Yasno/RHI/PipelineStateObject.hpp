#pragma once

#include <string>

#include <d3d12.h>
#include <wil/com.h>

namespace ysn
{
	using PipelineStateObjectId = uint64_t;

	class PipelineStateObject
	{
	public:
		PipelineStateObject(const std::wstring_view name) : m_name(name) {}

	private:
		const std::wstring	m_name = L"Unnamed PSO";

		ID3D12RootSignature* m_root_signature = nullptr;
		ID3D12PipelineState* m_pso = nullptr;
	};

	class GraphicsPipelineStateObject : public PipelineStateObject
	{
	};

	class ComputePipelineStateObject : public PipelineStateObject
	{
	};

	class PipelineStateObjectManager
	{
	public:

	private:
	};
}