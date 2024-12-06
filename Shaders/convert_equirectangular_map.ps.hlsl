struct PixelShaderInput
{
	float2 TexCoord : TexCoord;
};

Texture2D g_texture : register(t0);
SamplerState g_linear_sampler : register(s0);

float4 main(PixelShaderInput IN) : SV_Target
{
	return g_texture.Sample(g_linear_sampler, IN.TexCoord);
}
