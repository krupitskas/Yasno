#include "shader_structs.h"

struct IA2VS
{
	float3 position		: POSITION;
    float3 normal		: NORMAL;
    float2 texcoord_0	: TEXCOORD_0;
};

struct VS2RS
{
	float4 position		: SV_POSITION;
	float3 normal		: NORMAL;
    float2 texcoord_0	: TEXCOORD_0;
	float2 texcoord_1	: TEXCOORD_1;
};

ConstantBuffer<CameraParameters> parameters			: register(b0);
ConstantBuffer<GpuSceneParameters> scene_parameters	: register(b1);
ConstantBuffer<PerInstanceData> instance_data		: register(b2);

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

VS2RS main(IA2VS input)
{
	VS2RS output;

	float3x3 model_normal_matrix = transpose(Inverse3x3((float3x3)instance_data.model_matrix));

	output.position		= mul(instance_data.model_matrix, float4(input.position, 1.0));
	output.position		= mul(parameters.view_projection, output.position);
	output.normal		= mul(model_normal_matrix, float4(input.normal, 1.0));
    output.texcoord_0	= input.texcoord_0;

	return output;
}
