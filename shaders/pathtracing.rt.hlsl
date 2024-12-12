#include "shared.hlsl"

RWTexture2D<float4> result_texture							: register(u0);
RWTexture2D<float4> accumulation_texture					: register(u1);

ConstantBuffer<CameraParameters> camera						: register(b0);

RaytracingAccelerationStructure scene_bvh					: register(t0);
StructuredBuffer<VertexLayout> vertex_buffer				: register(t1);
StructuredBuffer<uint> index_buffer							: register(t2);
StructuredBuffer<SurfaceShaderParameters> materials_buffer	: register(t3);
StructuredBuffer<PerInstanceData> per_instance_buffer		: register(t4);

SamplerState linear_sampler									: register(s0);

[shader("raygeneration")]
void RayGen()
{
	result_texture[DispatchRaysIndex().xy] = float4(255, 0, 0, 0);

	// Initialize the ray payload
	RtxHitInfo payload;

	uint2 LaunchIndex = DispatchRaysIndex().xy;
	uint2 LaunchDimensions = DispatchRaysDimensions().xy;

	// Get the location within the dispatched 2D grid of work items
	// (often maps to pixels, so this could represent a pixel coordinate).
	uint2 pixel_xy = DispatchRaysIndex().xy;
	float2 resolution = float2(DispatchRaysDimensions().xy);


	float2 d = (((pixel_xy.xy + 0.5f) / resolution.xy) * 2.f - 1.f);

	float aspect_ratio = resolution.x / resolution.y;

	// Define a ray, consisting of origin, direction, and the min-max distance values
	RayDesc ray;
	ray.Origin = mul(camera.view_inverse, float4(0, 0, 0, 1));
	float4 target = mul(camera.projection_inverse, float4(d.x, -d.y, 1, 1));
	ray.Direction = mul(camera.view_inverse, float4(target.xyz, 0));
	ray.TMin = 0;
	ray.TMax = 100000;

	float3 radiance = float3(0.0f, 0.0f, 0.0f);
	float3 throughput = float3(1.0f, 1.0f, 1.0f);
	float3 sky_value = float3(1.0f, 1.0f, 1.0f);

	uint rng_state = InitRNG(LaunchIndex, LaunchDimensions, camera.frame_number);

	int max_bounces = 16;

	for (int bounce = 0; bounce < max_bounces; bounce++)
	{
		TraceRay(
			scene_bvh,
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
			result_texture[pixel_xy] = float4(0, 255, 0, 1.f);

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


	SurfaceShaderParameters mat = materials_buffer[payload.material_id];
	Texture2D albedo_texture = ResourceDescriptorHeap[mat.albedo_texture_index];
	float3 albedo_color = albedo_texture.SampleLevel(linear_sampler, payload.uvs, 0.0f).rgb;

	if(camera.reset_accumulation > 0)
	{
		accumulation_texture[pixel_xy] = 0;
	}

	float3 result_color = 0;

	if(camera.accumulation_enabled > 0)
	{
		// Temporal accumulation
		float3 previous_color = accumulation_texture[pixel_xy].rgb;
		float3 accumulated_color = radiance;
		accumulated_color = previous_color + radiance;
		accumulation_texture[pixel_xy] = float4(accumulated_color, 1.0f);

		result_color = accumulated_color / camera.rtx_frames_accumulated;
	}
	else
	{
		result_color = radiance;
	}

	result_texture[pixel_xy] = float4(result_color, 1.f);
}

uint3 GetIndices(uint geometry_id, uint triangle_index)
{
	PerInstanceData data = per_instance_buffer[geometry_id];

	int address = data.indices_before + triangle_index * 3;

	uint i0 = index_buffer[address + 0];
	uint i1 = index_buffer[address + 1];
	uint i2 = index_buffer[address + 2];

	return uint3(i0, i1, i2);
}

// Vertex data for usage in Hit shader
struct VertexData
{
	float3 position;
	float3 shading_normal;
	float3 geometry_normal;
	float2 uv;
};
VertexData GetVertexData(uint geometry_id, uint triangle_index, float3 barycentrics)
{
	// Get the triangle indices
	uint3 indices = GetIndices(geometry_id, triangle_index);

	VertexData v = (VertexData)0;

	VertexLayout input_vertex[3];

	float3 triangle_vertices[3];

	int address = per_instance_buffer[geometry_id].vertices_before;

	// Interpolate the vertex attributes
	for (uint i = 0; i < 3; i++)
	{
		input_vertex[i] = vertex_buffer[address + indices[i]];

		// Load and interpolate position and transform it to world space
		triangle_vertices[i] = mul(ObjectToWorld3x4(), float4(input_vertex[i].position.xyz, 1.0f)).xyz;

		v.position += triangle_vertices[i] * barycentrics[i];

		// Load and interpolate normal
		v.shading_normal += input_vertex[i].normal * barycentrics[i];

		// Load and interpolate texture coordinates
		v.uv += input_vertex[i].texcoord_0 * barycentrics[i];
	}

	// Transform normal from local to world space
	v.shading_normal = normalize(mul(ObjectToWorld3x4(), float4(v.shading_normal, 0.0f)).xyz);

	// Calculate geometry normal from triangle vertices positions
	float3 edge20 = triangle_vertices[2] - triangle_vertices[0];
	float3 edge21 = triangle_vertices[2] - triangle_vertices[1];
	float3 edge10 = triangle_vertices[1] - triangle_vertices[0];

	v.geometry_normal = normalize(cross(edge20, edge10));

	return v;
}

[shader("closesthit")]
void ClosestHit(inout RtxHitInfo payload, RtxAttributes attrib)
{
	float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

	VertexData vertex = GetVertexData(InstanceIndex(), PrimitiveIndex(), barycentrics);

	payload.encoded_normals = EncodeNormals(vertex.geometry_normal, vertex.shading_normal);
	payload.hit_position = vertex.position;
	payload.material_id = per_instance_buffer[InstanceIndex()].material_id;
	payload.uvs = vertex.uv;
}

[shader("miss")]
void Miss(inout RtxHitInfo payload : SV_RayPayload)
{
    payload.material_id = -1;
    payload.encoded_normals = float4(255,255,255,255);
}
