#include "shared.hlsl"

struct PixelShaderInput
{
	float2 TexCoord : TEXCOORD;
	float4 Position : SV_Position;
	float3 LocalPosition : POSITION;
};

TextureCube g_input_cubemap : register(t0);
SamplerState g_linear_sampler : register(s0);

float4 main(PixelShaderInput IN) : SV_Target
{
	const float3 normal = normalize(IN.LocalPosition);

    return float4(normal, 1.0);
}
