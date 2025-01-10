#include "shader_structs.h"

RWTexture2D<float4> HDRTexture : register(u0);
RWTexture2D<float4> LDRTexture : register(u1);

ConstantBuffer<TonemapParameters> parameters : register(b0);

[numthreads(8, 8, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
	if(threadId.x > parameters.display_width || threadId.y > parameters.display_height)
	{
		return;
	}

	const float gamma = 2.2;
	const float3 hdr_color = HDRTexture[threadId.xy];

	// This 'if' is not optimized but this is only for research purpose
	// In normal world should be ifdef or different shader code (ubershader)
	if (parameters.tonemap_method == 0) // None
	{
		LDRTexture[threadId.xy] = float4(hdr_color, 0.0);
		return;
	}
	else if (parameters.tonemap_method == 1) // Reinhard
	{
		const float3 ldr_color = 1.0f - exp(-hdr_color * parameters.exposure);
		const float3 ldr_gamma_corrected_color = pow(ldr_color, 1.0f / gamma);
		LDRTexture[threadId.xy] = float4(ldr_gamma_corrected_color, 0.0);

		return;
	}
	else if(parameters.tonemap_method == 2) // Aces
	{
		LDRTexture[threadId.xy] = float4(hdr_color, 0.0);
		return;
	}

	// Error magenta
	LDRTexture[threadId.xy] = float4(1, 0, 1, 1);
}
