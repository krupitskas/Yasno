struct IA2VS
{
	float3 position : POSITION;

#ifdef HAS_NORMAL
    float3 normal : NORMAL;
#endif

#ifdef HAS_TANGENT
    float4 tangent : TANGENT;
#endif

#ifdef HAS_TEXCOORD_0
    float2 texcoord_0: TEXCOORD_0;
#endif

#ifdef HAS_TEXCOORD_1
	float2 texcoord_1: TEXCOORD_1;
#endif
};

struct VS2RS
{
	float4 position : SV_POSITION;

#ifndef SHADOW_PASS
	float4 position_shadow_space : POSITION;
#endif

#ifdef HAS_NORMAL
	float3 normal : NORMAL;
#endif

#ifdef HAS_TANGENT
	float4 tangent : TANGENT;
#endif

#ifdef HAS_TEXCOORD_0
    float2 texcoord_0: TEXCOORD_0;
#endif

#ifdef HAS_TEXCOORD_1
	float2 texcoord_1: TEXCOORD_1;
#endif
};

#ifdef SHADOW_PASS
cbuffer ShadowCameraParameters : register(b0)
{
	float4x4 view_projection;
};
#else
cbuffer CameraParameters : register(b0)
{
	float4x4 view_projection;
	float4x4 view;
	float4x4 projection;
	float4x4 view_inverse;
	float4x4 projection_inverse;
	float3 camera_position;
};
#endif

cbuffer SceneNodeParameters : register(b1)
{
	float4x4 model_matrix;
};

cbuffer SceneParameters : register(b3)
{
	float4x4	shadow_matrix;
	float4		directional_light_color;
	float4		directional_light_direction;
	float		directional_light_intensity;
	float		ambient_light_intensity;
	uint		shadows_enabled;
};

VS2RS main(IA2VS input)
{
	VS2RS output;
	output.position = mul(float4(input.position, 1.0), model_matrix);

#ifndef SHADOW_PASS
	output.position_shadow_space = mul(shadow_matrix, output.position);
#endif

	output.position = mul(view_projection, output.position);

#ifdef HAS_NORMAL
	output.normal = mul(float4(input.normal, 1.0), model_matrix);
#endif

#ifdef HAS_TANGENT
	output.tangent.xyz = mul(float4(input.tangent.xyz, 1.0), model_matrix);
	output.tangent.w = input.tangent.w;
#endif

#ifdef HAS_TEXCOORD_0
    output.texcoord_0 = input.texcoord_0;
#endif

#ifdef HAS_TEXCOORD_1
	output.texcoord_1 = input.texcoord_1;
#endif

	return output;
}
