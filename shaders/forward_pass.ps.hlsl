#include "shader_structs.h"
#include "shared.hlsl"
#include "debug_renderer.hlsl"

struct RS2PS
{
	float4 position : SV_POSITION;
	float4 world_position : POSITION_0;
	float4 position_shadow_space : POSITION_1;
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
Texture2D	g_input_brdf			: register(t6);

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

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2.
float ndfGGX(float cosLh, float roughness)
{
	float alpha   = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float gaSchlickG1(float cosTheta, float k)
{
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float gaSchlickGGX(float cosLi, float cosLo, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return gaSchlickG1(cosLi, k) * gaSchlickG1(cosLo, k);
}

// Shlick's approximation of the Fresnel factor.
float3 fresnelSchlick(float3 F0, float cosTheta)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

//// Returns number of mipmap levels for specular IBL environment map.
//uint querySpecularTextureLevels()
//{
//	uint width, height, levels;
//	specularTexture.GetDimensions(0, width, height, levels);
//	return levels;
//}


static const float3 Fdielectric = 0.04;
static const float Epsilon = 0.00001;

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

	float4 albedo = pbr_material.base_color_factor;
	float metalness = pbr_material.metallic_factor;
	float roughness = pbr_material.roughness_factor;
	float3 normals = normal;
	float occlusion = 0;
	float3 emissive = 0;

	if (pbr_material.texture_enable_bitmask & (1 << ALBEDO_ENABLED_BIT))
	{
		Texture2D albedo_texture = ResourceDescriptorHeap[pbr_material.albedo_texture_index];
		albedo *= albedo_texture.Sample(g_linear_sampler, uv);

		//if(base_color.a < 0.1)
		//{
		//	discard;
		//}
	}

	if (pbr_material.texture_enable_bitmask & (1 << METALLIC_ROUGHNESS_ENABLED_BIT))
	{
		Texture2D metallic_roughness_texture = ResourceDescriptorHeap[pbr_material.metallic_roughness_texture_index];

		float4 metallicRoughnessResult = metallic_roughness_texture.Sample(g_linear_sampler, uv);

		metalness *= metallicRoughnessResult.b;
		roughness *= metallicRoughnessResult.g;
	}

	if (pbr_material.texture_enable_bitmask & (1 << NORMAL_ENABLED_BIT))
	{
		Texture2D normal_texture = ResourceDescriptorHeap[pbr_material.normal_texture_index];
		float3 texture_normals = normal_texture.Sample(g_linear_sampler, uv);
		texture_normals = 2.0f * texture_normals - 1.0f; // [0,1] ->[-1,1]

		normals *= texture_normals;
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

	// End of data fetch
	
	// Outgoing light direction (vector from world-space fragment position to the "eye").
	float3 Lo = normalize(camera.position.xyz - input.world_position.xyz);
	float3 N = normals;

	float cosLo = max(0.0, dot(N, Lo));
	float3 Lr = 2.0 * cosLo * N - Lo;

	float3 F0 = lerp(Fdielectric, albedo, metalness);

	float3 directLighting = 0.0;
	{
		float3 Li = -scene_parameters.directional_light_direction.xyz;
		float3 Lradiance = scene_parameters.directional_light_color.rgb * scene_parameters.directional_light_intensity;

		// Half-vector between Li and Lo.
		float3 Lh = normalize(Li + Lo);

		// Calculate angles between surface normal and various light vectors.
		float cosLi = max(0.0, dot(N, Li));
		float cosLh = max(0.0, dot(N, Lh));

		float3 F  = fresnelSchlick(F0, max(0.0, dot(Lh, Lo)));

		float D = ndfGGX(cosLh, roughness); // normal distribution

		float G = gaSchlickGGX(cosLi, cosLo, roughness);

		// Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
		// Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
		// To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
		float3 kd = lerp(float3(1, 1, 1) - F, float3(0, 0, 0), metalness);

		// Lambert diffuse BRDF.
		// We don't scale by 1/PI for lighting & material units to be more convenient.
		// See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
		float3 diffuseBRDF = kd * albedo;

		// Cook-Torrance specular microfacet BRDF.
		float3 specularBRDF = (F * D * G) / max(Epsilon, 4.0 * cosLi * cosLo);

		directLighting += (diffuseBRDF + specularBRDF) * Lradiance * cosLi;
	}


	float3 ambientLighting = 0.0;

	{
		float3 irradiance = g_input_irradiance.Sample(g_linear_sampler, N).rgb;
		float3 F = fresnelSchlick(F0, cosLo);

		float3 kd = lerp(1.0 - F, 0.0, metalness); // Get diffuse contribution factor (as with direct lighting).

		// Irradiance map contains exitant radiance assuming Lambertian BRDF, no need to scale by 1/PI here either.
		float3 diffuseIBL = kd * albedo * irradiance;

		// Sample pre-filtered specular reflection environment at correct mipmap level.
		uint specularTextureLevels = 8; // TODO: provide
		float3 specularIrradiance = g_input_radiance.SampleLevel(g_linear_sampler, Lr, roughness * specularTextureLevels).rgb;

		// Split-sum approximation factors for Cook-Torrance specular BRDF.
		float2 specularBRDF = g_input_brdf.Sample(g_linear_sampler, float2(cosLo, roughness)).rg; // special sampler

		float3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specularIrradiance;

		ambientLighting = diffuseIBL + specularIBL;
	}

	return float4(directLighting, 1.0); //  + ambientLighting
}
