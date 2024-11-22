//#include "Common.hlsl"

// Raytracing helpers below

// Hit information, aka ray payload
// This sample only carries a shading color and hit distance.
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
struct HitInfo
{
	float4 color_distance;
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct Attributes
{
	float2 bary;
};

struct VertexLayout
{
	float3 position;
    float3 normal;
    float4 tangent;
    float2 texcoord_0;
};

inline uint PackInstanceID(uint material_id, uint geometry_id)
{
	return ((geometry_id & 0x3FFF) << 10) | (material_id & 0x3FF);
}

inline void UnpackInstanceID(uint instanceID, out uint material_id, out uint geometry_id) 
{
	material_id = instanceID & 0x3FF;
	geometry_id = (instanceID >> 10) & 0x3FFF;
}

StructuredBuffer<VertexLayout> VertexBuffer		: register(t0);
StructuredBuffer<uint> IndexBuffer				: register(t1);

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	Texture2D albedo_texture = ResourceDescriptorHeap[3];

	uint primitive_index = PrimitiveIndex();
	
	float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

	uint material_id;
	uint geometry_id;
	UnpackInstanceID(primitive_index, material_id, geometry_id);
	
	//VertexLayout vertex_data = GetVertexAttributes(geometry_id, primitive_index, barycentrics);


	const float3 A = float3(1, 0, 0);
	const float3 B = float3(0, 1, 0);
	const float3 C = float3(0, 0, 1);

	float3 hit_color = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;

	payload.color_distance = float4(hit_color, RayTCurrent());
}
