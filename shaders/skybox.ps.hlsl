struct PixelShaderInput
{
    float4 Position             : SV_Position;
    float3 OriginalPosition     : POSITION;
    float2 TexCoord             : TEXCOORD;
};

TextureCube g_texture : register(t0);
SamplerState g_linear_sampler : register(s0);

float4 main(PixelShaderInput IN) : SV_Target
{
    return g_texture.Sample(g_linear_sampler, IN.OriginalPosition);
}
