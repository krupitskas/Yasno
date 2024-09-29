#pragma once

#include <DirectXMath.h>

namespace ysn
{
	#define ALBEDO_ENABLED_BIT				0
	#define METALLIC_ROUGHNESS_ENABLED_BIT	1
	#define NORMAL_ENABLED_BIT				2
	#define OCCLUSION_ENABLED_BIT			3
	#define EMISSIVE_ENABLED_BIT			4


	// Default PBR material
	struct SurfaceShaderParameters
	{
		DirectX::XMFLOAT4 base_color_factor;
		float metallic_factor;
		float roughness_factor;

		// Encods which textures are active
		int32_t texture_enable_bitmask;

		int32_t albedo_texture_index;
		int32_t metallic_roughness_texture_index;
		int32_t normal_texture_index;
		int32_t occlusion_texture_index;
		int32_t emissive_texture_index;
	};
}