struct VertexPosTexCoord
{
	float3 Position : POSITION;
	float2 TexCoord : TEXCOORD;
};

struct VertexShaderOutput
{
	float2 TexCoord : TEXCOORD;
	float4 Position : SV_Position;
	float3 LocalPosition : POSITION;
};

cbuffer ConvertCubemapCamera : register(b0)
{
	float4x4 view_projection;
};

VertexShaderOutput main(VertexPosTexCoord IN)
{
	VertexShaderOutput OUT;

	OUT.LocalPosition = IN.Position;
	OUT.Position = mul(view_projection, float4(IN.Position, 1.0f));
	OUT.TexCoord = IN.TexCoord;

	return OUT;
}
