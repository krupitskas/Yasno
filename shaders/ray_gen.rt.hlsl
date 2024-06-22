#include "shared.hlsl"

struct HitInfo
{
	float4 encoded_normals; // Octahedron encoded

	float3 hit_position;
	uint material_id;

	float2 uvs;

	bool has_hit() 
	{
		return material_id != -1;
	}
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct Attributes
{
	float2 bary;
};

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);
RWTexture2D<float4> AccumulationBuffer : register(u1);

cbuffer CameraParameters : register(b0)
{
  float4x4 view_projection;
  float4x4 view;
  float4x4 projection;
  float4x4 view_inverse;
  float4x4 projection_inverse;
  float3 position;
  uint frame_number;
  uint rtx_frames_accumulated;
  uint reset_accumulation;
  uint accumulation_enabled;
  uint pad;
}

// octahedron encoding of normals
float2 octWrap(float2 v)
{
	return float2((1.0f - abs(v.y)) * (v.x >= 0.0f ? 1.0f : -1.0f), (1.0f - abs(v.x)) * (v.y >= 0.0f ? 1.0f : -1.0f));
}

float2 encodeNormalOctahedron(float3 n)
{
	float2 p = float2(n.x, n.y) * (1.0f / (abs(n.x) + abs(n.y) + abs(n.z)));
	p = (n.z < 0.0f) ? octWrap(p) : p;
	return p;
}

float3 decodeNormalOctahedron(float2 p)
{
	float3 n = float3(p.x, p.y, 1.0f - abs(p.x) - abs(p.y));
	float2 tmp = (n.z < 0.0f) ? octWrap(float2(n.x, n.y)) : float2(n.x, n.y);
	n.x = tmp.x;
	n.y = tmp.y;
	return normalize(n);
}

float4 EncodeNormals(float3 geometryNormal, float3 shadingNormal) {
	return float4(encodeNormalOctahedron(geometryNormal), encodeNormalOctahedron(shadingNormal));
}

void DecodeNormals(float4 encodedNormals, out float3 geometryNormal, out float3 shadingNormal) {
	geometryNormal = decodeNormalOctahedron(encodedNormals.xy);
	shadingNormal = decodeNormalOctahedron(encodedNormals.zw);
}

//struct MaterialProperties
//{
//	float3 baseColor;
//	float metalness;

//	float3 emissive;
//	float roughness;

//	float transmissivness;
//	float opacity;
//};

//MaterialProperties LoadMaterialProperties(uint materialID, float2 uvs) {
//	MaterialProperties result = (MaterialProperties) 0;

//	// Read base data
//	MaterialData mData = materials[materialID];

//	result.baseColor = mData.baseColor;
//	result.emissive = mData.emissive;
//	result.metalness = mData.metalness;
//	result.roughness = mData.roughness;
//	result.opacity = mData.opacity;
	
//	// Load textures (using mip level 0)
//	if (mData.baseColorTexIdx != INVALID_ID) {
//		result.baseColor *= textures[mData.baseColorTexIdx].SampleLevel(linearSampler, uvs, 0.0f).rgb;
//	}

//	if (mData.emissiveTexIdx != INVALID_ID) {
//		result.emissive *= textures[mData.emissiveTexIdx].SampleLevel(linearSampler, uvs, 0.0f).rgb;
//	}

//	if (mData.roughnessMetalnessTexIdx != INVALID_ID) {
//		float3 occlusionRoughnessMetalness = textures[mData.roughnessMetalnessTexIdx].SampleLevel(linearSampler, uvs, 0.0f).rgb;
//		result.metalness *= occlusionRoughnessMetalness.b;
//		result.roughness *= occlusionRoughnessMetalness.g;
//	}

//	return result;
//}

// Clever offset_ray function from Ray Tracing Gems chapter 6
// Offsets the ray origin from current position p, along normal n (which must be geometric normal)
// so that no self-intersection can occur.
float3 OffsetRay(const float3 p, const float3 n)
{
	static const float origin = 1.0f / 32.0f;
	static const float float_scale = 1.0f / 65536.0f;
	static const float int_scale = 256.0f;

	int3 of_i = int3(int_scale * n.x, int_scale * n.y, int_scale * n.z);

	float3 p_i = float3(
		asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
		asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
		asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

	return float3(abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
		abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
		abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
}

#define PI 3.141592653589f
#define TWO_PI (2.0f * PI)
#define ONE_OVER_PI (1.0f / PI)
#define ONE_OVER_TWO_PI (1.0f / TWO_PI)

// -------------------------------------------------------------------------
//    Quaternion rotations
// -------------------------------------------------------------------------

// Calculates rotation quaternion from input vector to the vector (0, 0, 1)
// Input vector must be normalized!
float4 GetRotationToZAxis(float3 input) {

	// Handle special case when input is exact or near opposite of (0, 0, 1)
	if (input.z < -0.99999f) return float4(1.0f, 0.0f, 0.0f, 0.0f);

	return normalize(float4(input.y, -input.x, 0.0f, 1.0f + input.z));
}

// Calculates rotation quaternion from vector (0, 0, 1) to the input vector
// Input vector must be normalized!
float4 GetRotationFromZAxis(float3 input) {

	// Handle special case when input is exact or near opposite of (0, 0, 1)
	if (input.z < -0.99999f) return float4(1.0f, 0.0f, 0.0f, 0.0f);

	return normalize(float4(-input.y, input.x, 0.0f, 1.0f + input.z));
}

// Returns the quaternion with inverted rotation
float4 InvertRotation(float4 q)
{
	return float4(-q.x, -q.y, -q.z, q.w);
}

// Optimized point rotation using quaternion
// Source: https://gamedev.stackexchange.com/questions/28395/rotating-vector3-by-a-quaternion
float3 RotatePoint(float4 q, float3 v) {
	const float3 qAxis = float3(q.x, q.y, q.z);
	return 2.0f * dot(qAxis, v) * qAxis + (q.w * q.w - dot(qAxis, qAxis)) * v + 2.0f * q.w * cross(qAxis, v);
}

// Samples a direction within a hemisphere oriented along +Z axis with a cosine-weighted distribution 
// Source: "Sampling Transformations Zoo" in Ray Tracing Gems by Shirley et al.
float3 SampleHemisphere(float2 u, out float pdf)
{
	float a = sqrt(u.x);
	float b = TWO_PI * u.y;

	float3 result = float3(
		a * cos(b),
		a * sin(b),
		sqrt(1.0f - u.x));

	pdf = result.z * ONE_OVER_PI;

	return result;
}

float3 SampleHemisphere(float2 u) 
{
	float pdf;
	return SampleHemisphere(u, pdf);
}

// For sampling of all our diffuse BRDFs we use cosine-weighted hemisphere sampling, with PDF equal to (NdotL/PI)
float DiffusePdf(float NdotL) {
	return NdotL * ONE_OVER_PI;
}

float Luminance(float3 rgb)
{
	return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

struct VertexLayout
{
	float3 position;
    float3 normal;
    float4 tangent;
    float2 texcoord_0;
};
// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH	: register(t0);
StructuredBuffer<VertexLayout> VertexBuffer	: register(t1);
StructuredBuffer<uint> IndexBuffer			: register(t2);

// Material Buffer Data
struct SurfaceShaderParameters
{
	float4 base_color_factor;
	float metallic_factor;
	float roughness_factor;

	int texture_enable_bitmask; // Encoded which textures are should be used

	int albedo_texture_index;
	int metallic_roughness_texture_index;
	int normal_texture_index;
	int occlusion_texture_index;
	int emissive_texture_index;
};
StructuredBuffer<SurfaceShaderParameters> MaterialBuffer : register(t3);

struct PerInstanceData
{
	float4x4 model_matrix;
	int material_id;
	int vertices_before;
	int indices_before;
	int pad;
};
StructuredBuffer<PerInstanceData> PerInstanceBuffer : register(t4);

// Texture Sampler
SamplerState LinearSampler							: register(s0);

[shader("raygeneration")]
void RayGen()
{
	// Initialize the ray payload
	HitInfo payload;

	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;

	// Get the location within the dispatched 2D grid of work items
	// (often maps to pixels, so this could represent a pixel coordinate).
	uint2 pixel_xy = DispatchRaysIndex().xy;
	float2 resolution = float2(DispatchRaysDimensions().xy);

	// TODO: Anti Aliasing
	// float2 offset = float2(rand(rngState), rand(rngState));
	// pixel += lerp(-0.5.xx, 0.5.xx, offset);
	// pixel = (((pixel + 0.5f) / resolution) * 2.f - 1.f);
	// generatePinholeCameraRay(pixel);

	float2 d = (((pixel_xy.xy + 0.5f) / resolution.xy) * 2.f - 1.f);

	float aspect_ratio = resolution.x / resolution.y;

	// Define a ray, consisting of origin, direction, and the min-max distance values
	RayDesc ray;
	ray.Origin = mul(view_inverse, float4(0, 0, 0, 1));
	float4 target = mul(projection_inverse, float4(d.x, -d.y, 1, 1));
	ray.Direction = mul(view_inverse, float4(target.xyz, 0));
	ray.TMin = 0;
	ray.TMax = 100000;

	float3 radiance = float3(0.0f, 0.0f, 0.0f);
	float3 throughput = float3(1.0f, 1.0f, 1.0f);
	float3 sky_value = float3(1.0f, 1.0f, 1.0f);

	uint rng_state = InitRNG(LaunchIndex, LaunchDimensions, frame_number);

	int max_bounces = 16;

	for (int bounce = 0; bounce < max_bounces; bounce++)
	{
		TraceRay(
			SceneBVH,
			RAY_FLAG_NONE,
			0xFF,
			0,
			0,
			0,
			ray,
			payload
		);

		float2 uvs = float2(0.0f, 0.0f);

		if(!payload.has_hit())
		{
			radiance += throughput * sky_value;
			break;
		}

		float3 geometry_normal;
		float3 shading_normal;
		DecodeNormals(payload.encoded_normals, geometry_normal, shading_normal);

		float3 view_vec = -ray.Direction;

		if (dot(geometry_normal, view_vec) < 0.0f)
		{
			geometry_normal = -geometry_normal;
		}

		if (dot(geometry_normal, shading_normal) < 0.0f)
		{
			shading_normal = -shading_normal;
		}

		// Run importance sampling of selected BRDF to generate reflecting ray direction
		float3 brdf_weight;
		float2 u = float2(Rand(rng_state), Rand(rng_state));

		// Ray coming from "below" the hemisphere, goodbye
		if (dot(shading_normal, view_vec) <= 0.0f)
		{
			break;
		}

		// Transform view direction into local space of our sampling routines 
		// (local space is oriented so that its positive Z axis points along the shading normal)
		float4 quat_rot_to_z = GetRotationToZAxis(shading_normal);
		float3 v_local = RotatePoint(quat_rot_to_z, view_vec);
		const float3 n_local = float3(0.0f, 0.0f, 1.0f);

		float3 ray_direction_local = float3(0.0f, 0.0f, 0.0f);

		ray_direction_local = SampleHemisphere(u);

		//sampleWeight = data.diffuseReflectance * diffuseTerm(data);

		//// Prevent tracing direction with no contribution
		//if (Luminance(sampleWeight) == 0.0f)
		//{
		//	return false;
		//}

		// Transform sampled direction Llocal back to V vector space
		ray_direction_local = normalize(RotatePoint(InvertRotation(quat_rot_to_z), ray_direction_local));

		// Prevent tracing direction "under" the hemisphere (behind the triangle)
		if (dot(geometry_normal, ray_direction_local) <= 0.0f)
		{
			break;
		}

		// Update ray
		ray.Origin = OffsetRay(payload.hit_position, geometry_normal);
		ray.Direction = ray_direction_local;
	}


	//SurfaceShaderParameters mat = MaterialBuffer[payload.material_id];
	//Texture2D albedo_texture = ResourceDescriptorHeap[mat.albedo_texture_index];
	//float3 albedo_color = albedo_texture.SampleLevel(LinearSampler, payload.uvs, 0.0f).rgb;

	if(reset_accumulation > 0)
	{
		AccumulationBuffer[pixel_xy] = 0;
	}

	float3 result_color = 0;

	if(accumulation_enabled > 0)
	{
		// Temporal accumulation
		float3 previous_color = AccumulationBuffer[pixel_xy].rgb;
		float3 accumulated_color = radiance;
		accumulated_color = previous_color + radiance;
		AccumulationBuffer[pixel_xy] = float4(accumulated_color, 1.0f);

		result_color = accumulated_color / rtx_frames_accumulated;
	}
	else
	{
		result_color = radiance;
	}

	gOutput[pixel_xy] = float4(result_color, 1.f);
}
