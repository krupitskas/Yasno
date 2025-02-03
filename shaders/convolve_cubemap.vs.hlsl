#include "shader_structs.h"
#include "shared.hlsl"

struct VertexShaderOutput
{
	float2 TexCoord : TEXCOORD;
	float4 Position : SV_Position;
	float3 LocalPosition : POSITION;
};

cbuffer ViewMatrix : register(b0)
{
	float4x4 view;
};

cbuffer ProjectionMatrix : register(b1)
{
	float4x4 projection;
};

VertexShaderOutput main(VertexLayout IN)
{
	VertexShaderOutput OUT;

	OUT.LocalPosition = IN.position;
	OUT.Position = mul(projection, mul(view, float4(IN.position, 1.0f)));
	OUT.TexCoord = IN.texcoord_0;

	return OUT;
}
