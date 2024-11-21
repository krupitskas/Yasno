#pragma once

#include <DirectXMath.h>

#define ALBEDO_ENABLED_BIT				0
#define METALLIC_ROUGHNESS_ENABLED_BIT	1
#define NORMAL_ENABLED_BIT				2
#define OCCLUSION_ENABLED_BIT			3
#define EMISSIVE_ENABLED_BIT			4

namespace ysn
{
	// TODO: add
	// float3 emissive;
	// metallic_factor -> metalness
	// roughness_factor -> roughness
	// opacity
	// alphaMode 0: Opaque, 1: Blend, 2: Masked
	// alphaCutoff
	// doubleSided 0: false, 1: true
	
	// Default PBR material
	YSN_SHADER_STRUCT SurfaceShaderParameters
	{
		DirectX::XMFLOAT4 base_color_factor;
		float metallic_factor = 0.0f;
		float roughness_factor = 0.0f;

		// Encods which textures are active
		int32_t texture_enable_bitmask = 0;

		int32_t albedo_texture_index = 0;
		int32_t metallic_roughness_texture_index = 0;
		int32_t normal_texture_index = 0;
		int32_t occlusion_texture_index = 0;
		int32_t emissive_texture_index = 0;
	};
}