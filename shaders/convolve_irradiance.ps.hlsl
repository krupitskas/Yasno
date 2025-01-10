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

	float3 irradiance = 0.0f;

	float3 up = float3(0.0, 1.0, 0.0);
	const float3 right = normalize(cross(up, normal));
	up = normalize(cross(normal, right));

	const float sample_delta = 0.025f;
	int samples_num = 0; 

	for(float phi = 0.0; phi < 2.0 * PI; phi += sample_delta)
	{
		for(float theta = 0.0; theta < 0.5 * PI; theta += sample_delta)
		{
			// spherical to cartesian (in tangent space)
			float3 tangent_sample = float3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));

			// tangent space to world
			float3 sample_vec = tangent_sample.x * right + tangent_sample.y * up + tangent_sample.z * normal; 

			irradiance += g_input_cubemap.Sample(g_linear_sampler, sample_vec).rgb * cos(theta) * sin(theta);
			samples_num += 1;
		}
	}

	irradiance = PI * irradiance * (1.0 / float(samples_num));

	float3 test_irrd = g_input_cubemap.Sample(g_linear_sampler, normal).rgb ;

    return float4(test_irrd, 1.0);
}
