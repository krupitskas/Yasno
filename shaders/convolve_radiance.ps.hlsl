#include "shared.hlsl"

struct PixelShaderInput
{
	float2 TexCoord : TEXCOORD;
	float4 Position : SV_Position;
	float3 LocalPosition : POSITION;
};

TextureCube g_input_cubemap : register(t0);
SamplerState g_linear_sampler : register(s0);

cbuffer Parameters : register(b2)
{
	float roughness;
};

static const float Epsilon = 0.00001;
static const uint NumSamples = 1024;
static const float InvNumSamples = 1.0 / float(NumSamples);

// Compute Van der Corput radical inverse
// See: http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float radicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Sample i-th point from Hammersley point set of NumSamples points total.
float2 sampleHammersley(uint i)
{
	return float2(i * InvNumSamples, radicalInverse_VdC(i));
}

// Importance sample GGX normal distribution function for a fixed roughness value.
// This returns normalized half-vector between Li & Lo.
// For derivation see: http://blog.tobias-franke.eu/2014/03/30/notes_on_importance_sampling.html
float3 sampleGGX(float u1, float u2, float roughness)
{
	float alpha = roughness * roughness;

	float cosTheta = sqrt((1.0 - u2) / (1.0 + (alpha*alpha - 1.0) * u2));
	float sinTheta = sqrt(1.0 - cosTheta*cosTheta); // Trig. identity
	float phi = TWO_PI * u1;

	// Convert to Cartesian upon return.
	return float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2.
float ndfGGX(float cosLh, float roughness)
{
	float alpha   = roughness * roughness;
	float alphaSq = alpha * alpha;

	float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
	return alphaSq / (PI * denom * denom);
}

// Compute orthonormal basis for converting from tanget/shading space to world space.
void computeBasisVectors(const float3 N, out float3 S, out float3 T)
{
	// Branchless select non-degenerate T.
	T = cross(N, float3(0.0, 1.0, 0.0));
	T = lerp(cross(N, float3(1.0, 0.0, 0.0)), T, step(Epsilon, dot(T, T)));

	T = normalize(T);
	S = normalize(cross(N, T));
}

// Convert point from tangent/shading space to world space.
float3 tangentToWorld(const float3 v, const float3 N, const float3 S, const float3 T)
{
	return S * v.x + T * v.y + N * v.z;
}

float4 main(PixelShaderInput IN) : SV_Target
{
	uint MAX_MIPS = 8;

	const float inputWidth = 512;
	const float inputHeight = 512;

	// Solid angle associated with a single cubemap texel at zero mipmap level.
	// This will come in handy for importance sampling below.
	float wt = 4.0 * PI / (MAX_MIPS * inputWidth * inputHeight);
	
	// Approximation: Assume zero viewing angle (isotropic reflections).
	const float3 N = normalize(IN.LocalPosition) * float3(1, 1, -1); // TODO(LH to RH): Somewhere this is lost
	float3 Lo = N;
	
	float3 S, T;
	computeBasisVectors(N, S, T);

	float3 color = 0;
	float weight = 0;

	// Convolve environment map using GGX NDF importance sampling.
	// Weight by cosine term since Epic claims it generally improves quality.
	for(uint i=0; i<NumSamples; ++i) 
	{
		float2 u = sampleHammersley(i);
		float3 Lh = tangentToWorld(sampleGGX(u.x, u.y, roughness), N, S, T);

		// Compute incident direction (Li) by reflecting viewing direction (Lo) around half-vector (Lh).
		float3 Li = 2.0 * dot(Lo, Lh) * Lh - Lo;

		float cosLi = dot(N, Li);
		if(cosLi > 0.0) 
		{
			// Use Mipmap Filtered Importance Sampling to improve convergence.
			// See: https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html, section 20.4

			float cosLh = max(dot(N, Lh), 0.0);

			// GGX normal distribution function (D term) probability density function.
			// Scaling by 1/4 is due to change of density in terms of Lh to Li (and since N=V, rest of the scaling factor cancels out).
			float pdf = ndfGGX(cosLh, roughness) * 0.25;

			// Solid angle associated with this sample.
			float ws = 1.0 / (NumSamples * pdf);

			// Mip level to sample from.
			float mipLevel = max(0.5 * log2(ws / wt) + 1.0, 0.0);

			color  += g_input_cubemap.SampleLevel(g_linear_sampler, Li, mipLevel).rgb * cosLi;
			weight += cosLi;
		}
	}

	color /= weight;

	return float4(color, 1.0);
}