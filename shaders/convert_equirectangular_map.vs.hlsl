struct VertexPosTexCoord
{
	float3 Position : POSITION;
	float2 TexCoord : TEXCOORD;
};

struct VertexShaderOutput
{
	float2 TexCoord : TEXCOORD;
	float4 Position : SV_Position;
};

cbuffer CameraParameters : register(b0)
{
	float4x4 view_projection;
	float4x4 view;
	float4x4 projection;
	float4x4 view_inverse;
	float4x4 projection_inverse;
	float3 camera_position;
	uint frame_number;
	uint frames_accumulated;
    uint reset_accumulation;
    uint accumulation_enabled;
};

VertexShaderOutput main(VertexPosTexCoord IN)
{
	VertexShaderOutput OUT;

	OUT.Position = mul(view_projection, float4(IN.Position, 1.0f));
	OUT.TexCoord = IN.TexCoord;

	return OUT;
}
