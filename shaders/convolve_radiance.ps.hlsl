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
    const float roughness = 0.0;

	const float3 N = normalize(IN.LocalPosition);
    const float3 R = N;
    const float3 V = R;

    const uint SAMPLE_COUNT = 1024u;
    
    float total_weight = 0.0;   
    float3 prefiltered_color = 0.0f;     

    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H  = ImportanceSampleGGX(Xi, N, roughness);
        float3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0)
        {
            prefiltered_color += g_input_cubemap.Sample(g_linear_sampler, L).rgb * NdotL;
            total_weight      += NdotL;
        }
    }

    prefiltered_color = prefiltered_color / total_weight;

    return float4(prefiltered_color, 1.0);
}
