#pragma once

#include <Renderer/PipelineStateObject.hpp>
#include <Renderer/GpuResource.hpp>

namespace ysn
{
	struct Material
	{
		Material(std::string name) : name(name) {}

		std::string name = "Unnamed Material";

		D3D12_BLEND_DESC blend_desc;
		D3D12_RASTERIZER_DESC rasterizer_desc;

		GpuResource gpu_material_parameters;

		PipelineStateObjectId pso_id;
	};
}