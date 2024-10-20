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

};

StructuredBuffer<VertexLayout> VertexBuffer		: register(t0);
StructuredBuffer<uint> IndexBuffer				: register(t1);

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	Texture2D albedo_texture = ResourceDescriptorHeap[3];

	uint primitive_index = PrimitiveIndex();
	
	float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

	const float3 A = float3(1, 0, 0);
	const float3 B = float3(0, 1, 0);
	const float3 C = float3(0, 0, 1);

	float3 hit_color = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;

	payload.color_distance = float4(hit_color, RayTCurrent());
}
