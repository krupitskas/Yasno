#include "shader_structs.h"
#include "shared.hlsl"

struct IA2VS
{
	float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texcoord_0: TEXCOORD_0;
};

struct VS2RS
{
	float4 position : SV_POSITION;

#ifndef SHADOW_PASS
	float4 position_shadow_space : POSITION;
#endif

	float3 normal : NORMAL;
	float4 tangent : TANGENT;
    float2 texcoord_0: TEXCOORD_0;
};

#ifdef SHADOW_PASS
ConstantBuffer<ShadowCamera>		camera : register(b0);
#else
ConstantBuffer<CameraParameters>	camera : register(b0);
#endif

cbuffer SceneParameters : register(b1)
{
	float4x4	shadow_matrix;
	float4		directional_light_color;
	float4		directional_light_direction;
	float		directional_light_intensity;
	float		ambient_light_intensity;
	uint		shadows_enabled;
};

cbuffer InstanceId : register(b2)
{
    uint instance_id;
};

StructuredBuffer<PerInstanceData> per_instance_data : register(t0);

VS2RS main(IA2VS input)
{
	PerInstanceData instance_data = per_instance_data[instance_id];

	VS2RS output;
	output.position = mul(instance_data.model_matrix, float4(input.position, 1.0));

#ifndef SHADOW_PASS
	output.position_shadow_space = mul(shadow_matrix, output.position);
#endif

	output.position = mul(camera.view_projection, output.position);
	output.normal = mul(instance_data.model_matrix, float4(input.normal, 1.0)).xyz; // TODO: FIX IT
	output.tangent.xyz = mul(instance_data.model_matrix, float4(input.tangent.xyz, 1.0)).xyz;
	output.tangent.w = input.tangent.w;
    output.texcoord_0 = input.texcoord_0;

	return output;
}
