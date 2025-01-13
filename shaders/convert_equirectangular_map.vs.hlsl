#include "shared.hlsl"

struct VertexShaderOutput
{
	float4 Position			: SV_Position;
	float3 LocalPosition	: POSITION;
	float2 TexCoord			: TEXCOORD;
};

cbuffer ConvertCubemapCamera : register(b0)
{
	float4x4 view_projection;
};

VertexShaderOutput main(VertexLayout IN)
{
	VertexShaderOutput OUT;

	OUT.LocalPosition = IN.position;
	OUT.Position = mul(view_projection, float4(IN.position, 1.0f));
	OUT.TexCoord = IN.texcoord_0;

	return OUT;
}
