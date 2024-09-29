#pragma once

#include <Renderer/PipelineStateObject.hpp>

namespace ysn
{
	struct Material
	{
		Material(std::string name) : pso(name) {}

		PipelineStateObject pso;
		// wil::com_ptr<ID3D12RootSignature> pRootSignature;
		// wil::com_ptr<ID3D12PipelineState> pPipelineState;
	};
}