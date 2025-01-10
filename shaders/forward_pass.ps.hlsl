#include "shader_structs.h"
#include "shared.hlsl"

struct RS2PS
{
	float4 position : SV_POSITION;
	float4 position_shadow_space : POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 texcoord_0: TEXCOORD_0;
};

// Buffers
ConstantBuffer<CameraParameters> camera						: register(b0);
ConstantBuffer<GpuSceneParameters> scene_parameters			: register(b1);
ConstantBuffer<InstanceID> instance_id						: register(b2);

StructuredBuffer<PerInstanceData> per_instance_data			: register(t0);
StructuredBuffer<SurfaceShaderParameters> materials_buffer	: register(t1);

// Textures
Texture2D   g_shadow_map			: register(t2);
TextureCube g_input_cubemap			: register(t3);
TextureCube g_input_irradiance		: register(t4);
TextureCube g_input_radiance		: register(t5);

// Samplers
SamplerState g_shadow_sampler		: register(s0);
SamplerState g_linear_sampler		: register(s1);

float PCFShadowCalculation(float4 position)
{
	float shadow_value = 0.0;

	// TODO: dynamic shadowmap size, now it's 4096
	float2 texel_size = 1.0f / 4096.f;

	float3 projected_coordinate;
	projected_coordinate.x = position.x / position.w * 0.5 + 0.5;
	projected_coordinate.y = -position.y / position.w * 0.5 + 0.5;
	projected_coordinate.z = position.z / position.w;

	float current_depth = projected_coordinate.z + 0.005;

	for(int x = -1; x <= 1; ++x)
	{
		for(int y = -1; y <= 1; ++y)
		{
			float closest_depth = g_shadow_map.Sample(g_shadow_sampler, projected_coordinate.xy + float2(x, y) * texel_size).r;
			shadow_value += current_depth > closest_depth ? 1.0 : 0.0;        
		}    
	}
	shadow_value /= 9.0;

	return shadow_value;
}

float ShadowCalculation(float4 position)
{
	float3 projected_coordinate;
	projected_coordinate.x = position.x / position.w * 0.5 + 0.5;
	projected_coordinate.y = -position.y / position.w * 0.5 + 0.5;
	projected_coordinate.z = position.z / position.w;

	float closest_depth = g_shadow_map.Sample(g_shadow_sampler, projected_coordinate.xy);

	float current_depth = projected_coordinate.z + 0.005;

	float in_shadow = current_depth > closest_depth ? 1.0 : 0.0;  

	return in_shadow;
}

// Lambertian
float3 diffuse(float3 albedo, float3 light_color, float n_dot_l)
{
	return albedo * (light_color * max(n_dot_l, 0.0f));
}

float3 fresnel(float m, float3 light_color, float3 F0, float NdotH, float n_dot_l, float LdotH)
{
	float3 R = F0 + (1.0f - F0) * (1.0f - max(LdotH, 0.0f));
	R = R * ((m + 8) / 8) * pow(NdotH, m);
	return saturate(light_color * max(n_dot_l, 0.0f) * R);
}

float4 main(RS2PS input) : SV_Target
{
	float2 uv = float2(0.0, 0.0);
	float3 normal = 0;
	float4 tangent = 0;

	uv = input.texcoord_0;
	normal = input.normal;
	tangent = input.tangent;

	PerInstanceData instance_data = per_instance_data[instance_id.id];
	
	SurfaceShaderParameters pbr_material = materials_buffer[instance_data.material_id];

	float4 base_color = pbr_material.base_color_factor;
	float metallicResult = pbr_material.metallic_factor;
	float roughnessResult = pbr_material.roughness_factor;
	float3 normals = 1;
	float occlusion = 0;
	float3 emissive = 0;

	if (pbr_material.texture_enable_bitmask & (1 << ALBEDO_ENABLED_BIT))
	{
		Texture2D albedo_texture = ResourceDescriptorHeap[pbr_material.albedo_texture_index];
		base_color *= albedo_texture.Sample(g_linear_sampler, uv);

		if(base_color.a < 0.5)
		{
			discard;
		}
	}

	if (pbr_material.texture_enable_bitmask & (1 << METALLIC_ROUGHNESS_ENABLED_BIT))
	{
		Texture2D metallic_roughness_texture = ResourceDescriptorHeap[pbr_material.metallic_roughness_texture_index];

		float4 metallicRoughnessResult = metallic_roughness_texture.Sample(g_linear_sampler, uv);

		metallicResult *= metallicRoughnessResult.b;
		roughnessResult *= metallicRoughnessResult.g;
	}

	if (pbr_material.texture_enable_bitmask & (1 << NORMAL_ENABLED_BIT))
	{
		Texture2D normal_texture = ResourceDescriptorHeap[pbr_material.normal_texture_index];

		normals = normal_texture.Sample(g_linear_sampler, uv);
		normals = 2.0f * normals - 1.0f; // [0,1] ->[-1,1]
	}

	if (pbr_material.texture_enable_bitmask & (1 << OCCLUSION_ENABLED_BIT))
	{
		Texture2D occlusion_texture = ResourceDescriptorHeap[pbr_material.occlusion_texture_index];

		occlusion = occlusion_texture.Sample(g_linear_sampler, uv);
	}

	if (pbr_material.texture_enable_bitmask & (1 << EMISSIVE_ENABLED_BIT))
	{
		Texture2D emissive_texture = ResourceDescriptorHeap[pbr_material.emissive_texture_index];

		emissive = emissive_texture.Sample(g_linear_sampler, uv).rgb;
	}

	float3 V = normalize(camera.position.xyz - input.position.xyz); // From the shading location to the camera
	float3 L = scene_parameters.directional_light_direction.xyz; //normalize(LightPosition.xyz - input.position.xyz); // From the shading location to the LIGHT
	float3 N = normal; // Interpolated vertex normal

	//return float4(g_input_irradiance.Sample(g_linear_sampler, N)); // g_input_cubemap

	float n_dot_l = dot(N, L);

	float3 dieletricSpecular = { 0.04f, 0.04f, 0.04f };
	float3 black = { 0.0f, 0.0f, 0.0f };
	float3 C_ambient = scene_parameters.ambient_light_intensity * base_color;// * occlusion.x;
	float3 C_diff = lerp(base_color.rgb * (1.0f - dieletricSpecular.r), black, metallicResult);
	float3 C_diffuse = diffuse(C_diff, scene_parameters.directional_light_color.rgb * scene_parameters.directional_light_intensity, n_dot_l);

	//const float shadow_value = ShadowCalculation(input.position_shadow_space);
	const float shadow_value = PCFShadowCalculation(input.position_shadow_space);

	const float in_shadow = scene_parameters.shadows_enabled > 0 ? shadow_value : 1.0;

	float3 f = C_ambient + C_diffuse * in_shadow + emissive.xyz; //  + cubeMapSample.xyz + Specular

	return float4(f.xyz, 1.0);
}
