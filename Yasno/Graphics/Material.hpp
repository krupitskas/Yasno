#pragma once

#include <d3dx12.h>
#include <string>

namespace ysn
{
	struct Material
	{
		Material(std::string name) : name(name) {}

		std::string name = "Unnamed Material";

		D3D12_BLEND_DESC blend_desc = {};
		D3D12_RASTERIZER_DESC rasterizer_desc = {};
	};
}