#include "shader_structs.h"

Texture2D<float>	ShadowMapTexture	: register(t0);
Texture2D<float>	DepthTexture		: register(t1);
RWTexture2D<float4> HDRTexture			: register(u0);

ConstantBuffer<VolumetricFogParameters> fog_parameters		: register(b0);
ConstantBuffer<CameraParameters>		camera				: register(b1);
ConstantBuffer<GpuSceneParameters>		scene_parameters	: register(b2);

//float ShadowCalculation(float3 position)
//{
//	float3 projected_coordinate;
//	projected_coordinate.x = position.x / position.w * 0.5 + 0.5;
//	projected_coordinate.y = -position.y / position.w * 0.5 + 0.5;
//	projected_coordinate.z = position.z / position.w;

//	float closest_depth = g_shadow_map.Sample(g_shadow_sampler, projected_coordinate.xy);

//	float current_depth = projected_coordinate.z + 0.01;
//	float in_shadow = current_depth > closest_depth ? 1.0 : 0.0;  

//	return in_shadow;
//}

[numthreads(VolumetricFogDispatchX, VolumetricFogDispatchY, VolumetricFogDispatchZ)]
void main(uint3 threadId : SV_DispatchThreadID)
{
	if(threadId.x > fog_parameters.display_width || threadId.y > fog_parameters.display_height)
	{
		return;
	}

	const float2 uv = float2(threadId.xy) / float2(fog_parameters.display_width, fog_parameters.display_height);

	const float depth_value = DepthTexture[threadId.xy];

	if(depth_value > 0.9999)
	{
		return;
	}

	float2 clip_xy = uv * 2.0 - 1.0;
	clip_xy.y = -clip_xy.y;
	const float clip_z = depth_value;
	const float4 clip_pos = float4(clip_xy, clip_z, 1.0);

	float4 view_pos = mul(camera.projection_inverse, clip_pos);
	view_pos.xyz /= view_pos.w;

	const float3 world_pos = mul(camera.view_inverse, float4(view_pos.xyz, 1.0)).xyz;
	const float3 camera_pos = camera.position;

	const float raymarch_dist_limit = 999.0f;
	const float raymarch_distance = clamp(length(camera_pos - world_pos), 0.0f, raymarch_dist_limit);
	const float3 ray_dir = normalize(world_pos - camera_pos);

	const float ray_step_size = raymarch_distance / fog_parameters.num_steps;

	const int shadow_dim = 4096; // TODO: provide

	// Fog parameters
	float fog_density = 0.1f;
	float3 fog_color = 1.0f;

	float3 light_color = scene_parameters.directional_light_color.xyz * scene_parameters.directional_light_intensity;
	float3 light_dir = scene_parameters.directional_light_direction.xyz;

	// Accumulators
	float3 fog_accum = float3(0, 0, 0); // Accumulated fog color
	float transmittance = 1.0;          // Transmittance (how much light passes through)

    for (int i = 0; i < fog_parameters.num_steps; i++)
    {
		float t = i * ray_step_size;

		float3 sample_pos = camera_pos + ray_dir * t;
		
		float density = fog_density; // Replace with actual density sampling

		const float4 ray_pos_shadow_space = mul(scene_parameters.shadow_matrix, float4(sample_pos, 1.0f));

		float3 projected_coordinate;
		projected_coordinate.x = ray_pos_shadow_space.x / ray_pos_shadow_space.w * 0.5 + 0.5;
		projected_coordinate.y = -ray_pos_shadow_space.y / ray_pos_shadow_space.w * 0.5 + 0.5;
		projected_coordinate.z = ray_pos_shadow_space.z / ray_pos_shadow_space.w;

		int2 texel_coords = int2(projected_coordinate.xy * float2(shadow_dim, shadow_dim));

		const float shadow_depth = ShadowMapTexture.Load(int3(texel_coords, 0)).r;

		float fragment_depth = projected_coordinate.z;

		float bias = 0.01;
		bool in_shadow = fragment_depth - bias > shadow_depth;

		// Light attenuation (shadowing)
		float light_visibility = in_shadow ? 1.0 : 0.0;

		// Accumulate fog density and light contribution
		float step_transmittance = exp(-density * ray_step_size);
		fog_accum += light_color * light_visibility * density * transmittance * ray_step_size;
		transmittance *= step_transmittance;

		// Early exit if transmittance is too low
		if (transmittance < 0.01) 
			break;
    }

	HDRTexture[threadId.xy].xyz += fog_accum;
}
