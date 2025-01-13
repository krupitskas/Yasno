#include "shader_structs.h"
#include "shared.hlsl"

#ifndef SHADOW_PASS
#include "debug_renderer.hlsl"
#endif

struct IA2VS
{
	float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texcoord_0: TEXCOORD_0;
};

struct VertexToPixel
{
	float4 pixel_position : SV_POSITION;
	float4 world_position : POSITION_0;

#ifndef SHADOW_PASS
	float4 position_shadow_space : POSITION_1;
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

// TODO: prebake
float3x3 Inverse3x3(float3x3 mat)
{
    float3x3 inv;

    inv[0][0] = mat[1][1] * mat[2][2] - mat[2][1] * mat[1][2];
    inv[0][1] = mat[0][2] * mat[2][1] - mat[0][1] * mat[2][2];
    inv[0][2] = mat[0][1] * mat[1][2] - mat[0][2] * mat[1][1];

    inv[1][0] = mat[1][2] * mat[2][0] - mat[1][0] * mat[2][2];
    inv[1][1] = mat[0][0] * mat[2][2] - mat[0][2] * mat[2][0];
    inv[1][2] = mat[1][0] * mat[0][2] - mat[0][0] * mat[1][2];

    inv[2][0] = mat[1][0] * mat[2][1] - mat[2][0] * mat[1][1];
    inv[2][1] = mat[2][0] * mat[0][1] - mat[0][0] * mat[2][1];
    inv[2][2] = mat[0][0] * mat[1][1] - mat[1][0] * mat[0][1];

    float det = mat[0][0] * inv[0][0] + mat[0][1] * inv[1][0] + mat[0][2] * inv[2][0];
    return inv / det;
}

VertexToPixel main(IA2VS input)
{
	PerInstanceData instance_data = per_instance_data[instance_id];

	VertexToPixel output;
	output.world_position = mul(instance_data.model_matrix, float4(input.position, 1.0));

	float3x3 normal_matrix = transpose(Inverse3x3((float3x3)instance_data.model_matrix));
	output.normal = mul(normal_matrix, input.normal);

#ifndef SHADOW_PASS
	output.position_shadow_space = mul(shadow_matrix, output.world_position);
#endif

	output.pixel_position = mul(camera.view_projection, output.world_position);
	output.tangent.xyz = mul(instance_data.model_matrix, float4(input.tangent.xyz, 1.0)).xyz;
	output.tangent.w = input.tangent.w;
    output.texcoord_0 = input.texcoord_0;

	return output;
}
