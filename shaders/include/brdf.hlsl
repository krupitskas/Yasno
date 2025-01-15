// BRDF types
#define DIFFUSE_TYPE 1
#define SPECULAR_TYPE 2

// Specifies minimal reflectance for dielectrics (when metalness is zero)
// Nothing has lower reflectance than 2%, but we use 4% to have consistent results with UE4, Frostbite, et al.
#define MIN_DIELECTRICS_F0 0.04f

struct PTMaterialProperties
{
	float3 base_color;
	float metalness;

	float3 emissive;
	float roughness;

	float transmissivness;
	float opacity;
};

// Data needed to evaluate BRDF (surface and material properties at given point + configuration of light and normal vectors)
struct BrdfData
{
	// Material properties
	float3 specularF0;
	float3 diffuseReflectance;

	// Roughnesses
	float roughness;    //< perceptively linear roughness (artist's input)
	float alpha;        //< linear roughness - often 'alpha' in specular BRDF equations
	float alphaSquared; //< alpha squared - pre-calculated value commonly used in BRDF equations

	// Commonly used terms for BRDF evaluation
	float3 F; //< Fresnel term

	// Vectors
	float3 V; //< Direction to viewer (or opposite direction of incident ray)
	float3 N; //< Shading normal
	float3 H; //< Half vector (microfacet normal)
	float3 L; //< Direction to light (or direction of reflecting ray)

	float NdotL;
	float NdotV;

	float LdotH;
	float NdotH;
	float VdotH;

	// True when V/L is backfacing wrt. shading normal N
	bool Vbackfacing;
	bool Lbackfacing;
};


float3 BaseColorToSpecularF0(float3 baseColor, float metalness) {
	return lerp(float3(MIN_DIELECTRICS_F0, MIN_DIELECTRICS_F0, MIN_DIELECTRICS_F0), baseColor, metalness);
}

float3 BaseColorToDiffuseReflectance(float3 base_color, float metalness)
{
	return base_color * (1.0f - metalness);
}

// Specifies minimal reflectance for dielectrics (when metalness is zero)
// Nothing has lower reflectance than 2%, but we use 4% to have consistent results with UE4, Frostbite, et al.
#define MIN_DIELECTRICS_F0 0.04f

// Attenuates F90 for very low F0 values
// Source: "An efficient and Physically Plausible Real-Time Shading Model" in ShaderX7 by Schuler
// Also see section "Overbright highlights" in Hoffman's 2010 "Crafting Physically Motivated Shading Models for Game Development" for discussion
// IMPORTANT: Note that when F0 is calculated using metalness, it's value is never less than MIN_DIELECTRICS_F0, and therefore,
// this adjustment has no effect. To be effective, F0 must be authored separately, or calculated in different way. See main text for discussion.
float ShadowedF90(float3 F0) {
	// This scaler value is somewhat arbitrary, Schuler used 60 in his article. In here, we derive it from MIN_DIELECTRICS_F0 so
	// that it takes effect for any reflectance lower than least reflective dielectrics
	//const float t = 60.0f;
	const float t = (1.0f / MIN_DIELECTRICS_F0);
	return min(1.0f, t * Luminance(F0));
}

// Schlick's approximation to Fresnel term
// f90 should be 1.0, except for the trick used by Schuler (see 'shadowedF90' function)
float3 EvalFresnelSchlick(float3 f0, float f90, float n_dot_s)
{
	return f0 + (f90 - f0) * pow(1.0f - n_dot_s, 5.0f);
}

float3 EvalFresnel(float3 f0, float f90, float n_dot_s)
{
	// Default is Schlick's approximation
	return EvalFresnelSchlick(f0, f90, n_dot_s);
}

// Calculates probability of selecting BRDF (specular or diffuse) using the approximate Fresnel term
float GetBrdfProbability(PTMaterialProperties material, float3 view_vec, float3 shading_normal) {
	
	// Evaluate Fresnel term using the shading normal
	// Note: we use the shading normal instead of the microfacet normal (half-vector) for Fresnel term here. That's suboptimal for rough surfaces at grazing angles, but half-vector is yet unknown at this point
	float specular_f0 = Luminance(BaseColorToSpecularF0(material.base_color, material.metalness));
	float diffuse_reflectance = Luminance(BaseColorToDiffuseReflectance(material.base_color, material.metalness));
	float fresnel = saturate(Luminance(EvalFresnel(specular_f0, ShadowedF90(specular_f0), max(0.0f, dot(view_vec, shading_normal)))));

	// Approximate relative contribution of BRDFs using the Fresnel term
	float specular = fresnel;
	float diffuse = diffuse_reflectance * (1.0f - fresnel); //< If diffuse term is weighted by Fresnel, apply it here as well

	// Return probability of selecting specular BRDF over diffuse BRDF
	float p = (specular / max(0.0001f, (specular + diffuse)));

	// Clamp probability to avoid undersampling of less prominent BRDF
	return clamp(p, 0.1f, 0.9f);
}


// Precalculates commonly used terms in BRDF evaluation
// Clamps around dot products prevent NaNs and ensure numerical stability, but make sure to
// correctly ignore rays outside of the sampling hemisphere, by using 'Vbackfacing' and 'Lbackfacing' flags
BrdfData PrepareBRDFData(float3 N, float3 L, float3 V, PTMaterialProperties material) {
	BrdfData data;

	// Evaluate VNHL vectors
	data.V = V;
	data.N = N;
	data.H = normalize(L + V);
	data.L = L;

	float NdotL = dot(N, L);
	float NdotV = dot(N, V);
	data.Vbackfacing = (NdotV <= 0.0f);
	data.Lbackfacing = (NdotL <= 0.0f);

	// Clamp NdotS to prevent numerical instability. Assume vectors below the hemisphere will be filtered using 'Vbackfacing' and 'Lbackfacing' flags
	data.NdotL = min(max(0.00001f, NdotL), 1.0f);
	data.NdotV = min(max(0.00001f, NdotV), 1.0f);

	data.LdotH = saturate(dot(L, data.H));
	data.NdotH = saturate(dot(N, data.H));
	data.VdotH = saturate(dot(V, data.H));

	// Unpack material properties
	data.specularF0 = BaseColorToSpecularF0(material.base_color, material.metalness);
	data.diffuseReflectance = BaseColorToDiffuseReflectance(material.base_color, material.metalness);

	// Unpack 'perceptively linear' -> 'linear' -> 'squared' roughness
	data.roughness = material.roughness;
	data.alpha = material.roughness * material.roughness;
	data.alphaSquared = data.alpha * data.alpha;

	// Pre-calculate some more BRDF terms
	data.F = EvalFresnel(data.specularF0, ShadowedF90(data.specularF0), data.LdotH);

	return data;
}

// Lambertian
float DiffuseTerm(const BrdfData data) {
	return 1.0f;
}

// Samples a microfacet normal for the GGX distribution using VNDF method.
// Source: "Sampling the GGX Distribution of Visible Normals" by Heitz
// See also https://hal.inria.fr/hal-00996995v1/document and http://jcgt.org/published/0007/04/01/
// Random variables 'u' must be in <0;1) interval
// PDF is 'G1(NdotV) * D'
float3 SampleGGXVNDF(float3 Ve, float2 alpha2D, float2 u) {

	// Section 3.2: transforming the view direction to the hemisphere configuration
	float3 Vh = normalize(float3(alpha2D.x * Ve.x, alpha2D.y * Ve.y, Ve.z));

	// Section 4.1: orthonormal basis (with special case if cross product is zero)
	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	float3 T1 = lensq > 0.0f ? float3(-Vh.y, Vh.x, 0.0f) * rsqrt(lensq) : float3(1.0f, 0.0f, 0.0f);
	float3 T2 = cross(Vh, T1);

	// Section 4.2: parameterization of the projected area
	float r = sqrt(u.x);
	float phi = TWO_PI * u.y;
	float t1 = r * cos(phi);
	float t2 = r * sin(phi);
	float s = 0.5f * (1.0f + Vh.z);
	t2 = lerp(sqrt(1.0f - t1 * t1), t2, s);

	// Section 4.3: reprojection onto hemisphere
	float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0f, 1.0f - t1 * t1 - t2 * t2)) * Vh;

	// Section 3.4: transforming the normal back to the ellipsoid configuration
	return normalize(float3(alpha2D.x * Nh.x, alpha2D.y * Nh.y, max(0.0f, Nh.z)));
}

#define SampleSpecularHalfVector SampleGGXVNDF

// Smith G1 term (masking function) further optimized for GGX distribution (by substituting G_a into G1_GGX)
float Smith_G1_GGX(float alpha, float NdotS, float alphaSquared, float NdotSSquared) {
	return 2.0f / (sqrt(((alphaSquared * (1.0f - NdotSSquared)) + NdotSSquared) / NdotSSquared) + 1.0f);
}

#define Smith_G1 Smith_G1_GGX

// A fraction G2/G1 where G2 is height correlated can be expressed using only G1 terms
// Source: "Implementing a Simple Anisotropic Rough Diffuse Material with Stochastic Evaluation", Appendix A by Heitz & Dupuy
float Smith_G2_Over_G1_Height_Correlated(float alpha, float alphaSquared, float NdotL, float NdotV) {
	float G1V = Smith_G1(alpha, NdotV, alphaSquared, NdotV * NdotV);
	float G1L = Smith_G1(alpha, NdotL, alphaSquared, NdotL * NdotL);
	return G1L / (G1V + G1L - G1V * G1L);
}

// Weight for the reflection ray sampled from GGX distribution using VNDF method
float SpecularSampleWeightGGXVNDF(float alpha, float alphaSquared, float NdotL, float NdotV, float HdotL, float NdotH) {
	return Smith_G2_Over_G1_Height_Correlated(alpha, alphaSquared, NdotL, NdotV);
}

#define SpecularSampleWeight SpecularSampleWeightGGXVNDF

// Samples a reflection ray from the rough surface using selected microfacet distribution and sampling method
// Resulting weight includes multiplication by cosine (NdotL) term
float3 SampleSpecularMicrofacet(float3 Vlocal, float alpha, float alphaSquared, float3 specularF0, float2 u, out float3 weight) {

	// Sample a microfacet normal (H) in local space
	float3 Hlocal;
	if (alpha == 0.0f) {
		// Fast path for zero roughness (perfect reflection), also prevents NaNs appearing due to divisions by zeroes
		Hlocal = float3(0.0f, 0.0f, 1.0f);
	} else {
		// For non-zero roughness, this calls VNDF sampling for GG-X distribution or Walter's sampling for Beckmann distribution
		Hlocal = SampleSpecularHalfVector(Vlocal, float2(alpha, alpha), u);
	}

	// Reflect view direction to obtain light vector
	float3 Llocal = reflect(-Vlocal, Hlocal);

	// Note: HdotL is same as HdotV here
	// Clamp dot products here to small value to prevent numerical instability. Assume that rays incident from below the hemisphere have been filtered
	float HdotL = max(0.00001f, min(1.0f, dot(Hlocal, Llocal)));
	const float3 Nlocal = float3(0.0f, 0.0f, 1.0f);
	float NdotL = max(0.00001f, min(1.0f, dot(Nlocal, Llocal)));
	float NdotV = max(0.00001f, min(1.0f, dot(Nlocal, Vlocal)));
	float NdotH = max(0.00001f, min(1.0f, dot(Nlocal, Hlocal)));
	float3 F = EvalFresnel(specularF0, ShadowedF90(specularF0), HdotL);

	// Calculate weight of the sample specific for selected sampling method 
	// (this is microfacet BRDF divided by PDF of sampling method - notice how most terms cancel out)
	weight = F * SpecularSampleWeight(alpha, alphaSquared, NdotL, NdotV, HdotL, NdotH);

	return Llocal;
}

#define SampleSpecular SampleSpecularMicrofacet

// This is an entry point for evaluation of all other BRDFs based on selected configuration (for indirect light)
bool EvalIndirectCombinedBRDF(	float2 u,
								float3 shading_normal,
								float3 geometry_normal,
								float3 view_vec,
								PTMaterialProperties material,
								const int brdf_type,
								out float3 ray_direction,
								out float3 sample_weight)
{

	// Ignore incident ray coming from "below" the hemisphere
	if (dot(shading_normal, view_vec) <= 0.0f)
		return false;

	// Transform view direction into local space of our sampling routines 
	// (local space is oriented so that its positive Z axis points along the shading normal)
	float4 qRotationToZ = GetRotationToZAxis(shading_normal);
	float3 view_vec_local = RotatePoint(qRotationToZ, view_vec);
	const float3 n_local = float3(0.0f, 0.0f, 1.0f);

	float3 ray_direction_local = float3(0.0f, 0.0f, 0.0f);

	if (brdf_type == DIFFUSE_TYPE)
	{

		// Sample diffuse ray using cosine-weighted hemisphere sampling 
		ray_direction_local = SampleHemisphere(u);
		const BrdfData data = PrepareBRDFData(n_local, ray_direction_local, view_vec_local, material);

		// Function 'diffuseTerm' is predivided by PDF of sampling the cosine weighted hemisphere
		sample_weight = data.diffuseReflectance * DiffuseTerm(data);

#if COMBINE_BRDFS_WITH_FRESNEL		
		// Sample a half-vector of specular BRDF. Note that we're reusing random variable 'u' here, but correctly it should be an new independent random number
		float3 Hspecular = SampleSpecularHalfVector(Vlocal, float2(data.alpha, data.alpha), u);

		// Clamp HdotL to small value to prevent numerical instability. Assume that rays incident from below the hemisphere have been filtered
		float VdotH = max(0.00001f, min(1.0f, dot(Vlocal, Hspecular)));
		sample_weight *= (float3(1.0f, 1.0f, 1.0f) - EvalFresnel(data.specularF0, ShadowedF90(data.specularF0), VdotH));
#endif

	}
	else if (brdf_type == SPECULAR_TYPE)
	{
		const BrdfData data = PrepareBRDFData(n_local, float3(0.0f, 0.0f, 1.0f) /* unused L vector */, view_vec_local, material);
		ray_direction_local = SampleSpecular(view_vec_local, data.alpha, data.alphaSquared, data.specularF0, u, sample_weight);
	}

	// Prevent tracing direction with no contribution
	if (Luminance(sample_weight) == 0.0f)
		return false;

	// Transform sampled direction Llocal back to V vector space
	ray_direction = normalize(RotatePoint(InvertRotation(qRotationToZ), ray_direction_local));

	// Prevent tracing direction "under" the hemisphere (behind the triangle)
	if (dot(geometry_normal, ray_direction) <= 0.0f) return false;

	return true;
}