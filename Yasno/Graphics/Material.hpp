#pragma once

#include <Renderer/PSO.hpp>
#include <Renderer/GpuResource.hpp>
#include <Graphics/SurfaceMaterial.hpp>

namespace ysn
{
	struct Material
	{
		Material(std::string name) : name(name) {}

		std::string name = "Unnamed Material";

		D3D12_BLEND_DESC blend_desc = {};
		D3D12_RASTERIZER_DESC rasterizer_desc = {};

		SurfaceShaderParameters shader_parameters;
	};
}