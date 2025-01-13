#include "shared.hlsl"

struct PixelShaderInput
{
	float2 TexCoord : TEXCOORD;
	float4 Position : SV_Position;
	float3 LocalPosition : POSITION;
};

TextureCube g_input_cubemap : register(t0);
SamplerState g_linear_sampler : register(s0);

float3 ImportanceSampleHemisphere(float2 xi, float3 n) {
    // Convert 2D random point to hemisphere sample (cosine-weighted)
    float phi = 2.0 * 3.14159265359 * xi.x;
    float cosTheta = sqrt(1.0 - xi.y);
    float sinTheta = sqrt(xi.y);

    float3 tangent = abs(n.z) < 0.999 ? normalize(cross(float3(0.0, 0.0, 1.0), n)) : float3(1.0, 0.0, 0.0);
    float3 bitangent = cross(n, tangent);

    return cosTheta * n + sinTheta * (cos(phi) * tangent + sin(phi) * bitangent);
}

float4 main(PixelShaderInput IN) : SV_Target
{
    float3 normal = normalize(IN.LocalPosition);

    float3 test = g_input_cubemap.Sample(g_linear_sampler, normal).rgb;
    return float4(test, 1.0);



    const int NumSamples = 64;
    float3 irradiance = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < NumSamples; ++i) {
        float2 xi = Hammersley(i, NumSamples);
        float3 sampleDir = ImportanceSampleHemisphere(xi, normal);

        irradiance += g_input_cubemap.Sample(g_linear_sampler, sampleDir).rgb;
    }

    irradiance /= NumSamples;
    return float4(irradiance, 1.0);

}
