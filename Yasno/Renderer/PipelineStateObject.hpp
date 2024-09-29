#pragma once

#include <string>
#include <unordered_map>

#include <d3d12.h>
#include <wil/com.h>

namespace ysn
{
	using PipelineStateObjectId = uint64_t;

	struct PipelineStateObject
	{
		PipelineStateObject(const std::string name) : m_name(name) {}

	private:
		const std::string m_name = "Unnamed PSO";

		ID3D12RootSignature* m_root_signature = nullptr;
		ID3D12PipelineState* m_pso = nullptr;
	};
}