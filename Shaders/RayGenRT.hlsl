//#include "Common.hlsl"

struct HitInfo
{
	float4 color_distance;

	bool has_hit;
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct Attributes
{
	float2 bary;
};

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);

cbuffer CameraParameters : register(b0)
{
  float4x4 view_projection;
  float4x4 view;
  float4x4 projection;
  float4x4 view_inverse;
  float4x4 projection_inverse;
  float3 position;
}

uint JenkinsHash(uint x)
{
	x += x << 10;
	x ^= x >> 6;
	x += x << 3;
	x ^= x >> 11;
	x += x << 15;
	return x;
}

uint InitRNG(uint2 pixel , uint2 resolution , uint frame) 
{
	const uint rng_state = dot(pixel , uint2(1, resolution.x)) ^ JenkinsHash(frame);
	return JenkinsHash(rng_state);
}

float UintToFloat(uint x) 
{
	return asfloat(0x3f800000 | (x >> 9)) - 1.f;
}

uint Xorshift(inout uint rng_state)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

float Rand(inout uint rng_state) 
{
	return UintToFloat(Xorshift(rng_state));
}

// input : pixel screen space coordinate, frame number, sample number
// return : four random numbers
uint4 Pcg4d(uint4 v)
{
	v = v * 1664525u + 1013904223u;
	
	v.x += v.y * v.w;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v.w += v.y * v.z;
	
	v = v ^ (v >> 16u);
	v.x += v.y * v.w;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v.w += v.y * v.z;
	
	return v;
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
	int indices_count;
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
	payload.color_distance = float4(0, 0, 0, 0);

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

	TraceRay(SceneBVH,
		RAY_FLAG_NONE,
		0xFF,
		0,
		0,
		0,
		ray,
		payload
	);

	SurfaceShaderParameters mat = MaterialBuffer[0];

	Texture2D albedo_texture = ResourceDescriptorHeap[mat.albedo_texture_index];

	float2 uvs = float2(0.0f, 0.0f);

	float3 albedo_color = albedo_texture.SampleLevel(LinearSampler, uvs, 0.0f).rgb;

	gOutput[pixel_xy] = float4(payload.color_distance.rgb * albedo_color.rgb, 1.f);
}
