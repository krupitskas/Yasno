struct RS2PS
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 texcoord_0: TEXCOORD_0;
};

float4 main(RS2PS input) : SV_Target
{
	return float4(1.0f, 0.0f, 0.0f, 0.0f);
}
