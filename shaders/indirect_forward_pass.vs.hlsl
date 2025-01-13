#include "shader_structs.h"

struct IA2VS
{
	float3 position : POSITION;
    float3 normal : NORMAL;
    //float4 tangent : TANGENT;
    float2 texcoord_0: TEXCOORD_0;
};

struct VS2RS
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	//float4 tangent : TANGENT;
    float2 texcoord_0: TEXCOORD_0;
	float2 texcoord_1: TEXCOORD_1;
};

ConstantBuffer<CameraParameters> parameters			: register(b0);
ConstantBuffer<GpuSceneParameters> scene_parameters	: register(b1);
ConstantBuffer<PerInstanceData> instance_data		: register(b2);

VS2RS main(IA2VS input)
{
	VS2RS output;

	output.position = mul(float4(input.position, 1.0), instance_data.model_matrix);
	output.position = mul(parameters.view_projection, output.position);
	output.normal = mul(float4(input.normal, 1.0), instance_data.model_matrix);
	//output.tangent.xyz = mul(float4(input.tangent.xyz, 1.0), instance_data.model_matrix);
	//output.tangent.w = input.tangent.w;
    output.texcoord_0 = input.texcoord_0;

	return output;
}
