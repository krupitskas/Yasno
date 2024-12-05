RWTexture2D<float4> HDRTexture : register(u0);
RWTexture2D<float4> LDRTexture : register(u1);

cbuffer Parameters : register(b0)
{
	uint display_width;
	uint display_height;
	uint tonemap_method;
	float exposure;
};

[numthreads(8, 8, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
	if(threadId.x > display_width || threadId.y > display_height)
	{
		return;
	}

	const float gamma = 2.2;

	const float3 hdr_color = HDRTexture[threadId.xy];

	// Reinhard
	const float3 ldr_color = 1.0f - exp(-hdr_color * exposure);
	const float3 ldr_gamma_corrected_color = pow(ldr_color, 1.0f / gamma);

	LDRTexture[threadId.xy] = float4(ldr_gamma_corrected_color, 0.0);
}
