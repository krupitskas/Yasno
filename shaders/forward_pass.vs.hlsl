#include "shader_structs.h"
#include "shared.hlsl"

#ifndef SHADOW_PASS
#include "debug_renderer.hlsl"
#endif

struct VertexToPixel
{
#ifndef SHADOW_PASS
	float4 position_shadow_space : POSITION_1;
#endif

	float4 pixel_position : SV_POSITION;
	float4 world_position : POSITION_0;
	float3 normal : NORMAL;
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

ConstantBuffer<InstanceID> instance_id				: register(b2);

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

VertexToPixel main(VertexLayout input)
{
	PerInstanceData instance_data = per_instance_data[instance_id.id];

	float3x3 normal_matrix = transpose(Inverse3x3((float3x3)instance_data.model_matrix));
	
	VertexToPixel output;
	output.world_position	= mul(instance_data.model_matrix, float4(input.position, 1.0));
	output.normal			= mul(normal_matrix, input.normal);
	output.pixel_position	= mul(camera.view_projection, output.world_position);
    output.texcoord_0		= input.texcoord_0;

#ifndef SHADOW_PASS
	output.position_shadow_space = mul(shadow_matrix, output.world_position);

	//if(instance_id.id == 55)
	//{
	//	DrawLine(output.world_position, output.world_position + normalize(output.normal) * 0.25, float3(255, 0 ,0));
	//}

#endif

	return output;
}
