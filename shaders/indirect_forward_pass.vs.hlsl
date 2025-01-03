#include "shader_structs.h"

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
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
    float2 texcoord_0: TEXCOORD_0;
	float2 texcoord_1: TEXCOORD_1;
};

ConstantBuffer<CameraParameters> parameters : register(b0);

cbuffer SceneParameters : register(b1)
{
	float4x4	shadow_matrix;
	float4		directional_light_color;
	float4		directional_light_direction;
	float		directional_light_intensity;
	float		ambient_light_intensity;
	uint		shadows_enabled;
};

cbuffer PerInstanceData : register(b2)
{
	float4x4 model_matrix;
	int material_id;
	int vertices_offset;
	int indices_offset;
	int pad;
};

VS2RS main(IA2VS input)
{
	VS2RS output;

	output.position = mul(float4(input.position, 1.0), model_matrix);
	output.position = mul(parameters.view_projection, output.position);
	output.normal = mul(float4(input.normal, 1.0), model_matrix);
	output.tangent.xyz = mul(float4(input.tangent.xyz, 1.0), model_matrix);
	output.tangent.w = input.tangent.w;
    output.texcoord_0 = input.texcoord_0;

	return output;
}
