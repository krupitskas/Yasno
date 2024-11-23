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

inline uint PackInstanceID(uint material_id, uint geometry_id)
{
	return ((geometry_id & 0x3FFF) << 10) | (material_id & 0x3FF);
}

inline void UnpackInstanceID(uint instanceID, out uint material_id, out uint geometry_id) 
{
	material_id = instanceID & 0x3FF;
	geometry_id = (instanceID >> 10) & 0x3FFF;
}

// Vertex data for usage in Hit shader
struct VertexData
{
	float3 position;
	float3 shading_normal;
	float3 geometry_normal;
	float2 uv;
};

// Vertex buffer layout data
struct VertexLayout
{
	float3 position;
    float3 normal;
    float4 tangent;
    float2 texcoord_0;
};
StructuredBuffer<VertexLayout> VertexBuffer				: register(t1);

// Index Buffer data
StructuredBuffer<uint> IndexBuffer						: register(t2);

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

// Per instance data
struct PerInstanceData
{
	float4x4 model_matrix;
	int material_id;
	int indices_count;
	int indices_before;
	int pad;
};
StructuredBuffer<PerInstanceData> PerInstanceBuffer : register(t4);

uint3 GetIndices(uint geometry_id, uint triangle_index)
{
	PerInstanceData data = PerInstanceBuffer[geometry_id];

	uint base_index = (triangle_index * 3);
	int address = data.indices_before + base_index;

	uint i0 = IndexBuffer[address];
	uint i1 = IndexBuffer[address + 1];
	uint i2 = IndexBuffer[address + 2];

	return uint3(i0, i1, i2);
}

VertexData GetVertexData(uint geometry_id, uint triangle_index, float3 barycentrics)
{
	// Get the triangle indices
	uint3 indices = GetIndices(geometry_id, triangle_index);

	VertexData v = (VertexData)0;

	float3 triangle_vertices[3];

	// Interpolate the vertex attributes
	//for (uint i = 0; i < 3; i++)
	//{
	//	int address = (indices[i] * 12) * 4;
	//
	//	// Load and interpolate position and transform it to world space
	//	triangle_vertices[i] = mul(ObjectToWorld3x4(), float4(asfloat(VertexBuffer[geometry_id].Load3(address)), 1.0f)).xyz;
	//	v.position += triangle_vertices[i] * barycentrics[i];
	//	address += 12;
	//
	//	// Load and interpolate normal
	//	v.normal += asfloat(VertexBuffer[geometry_id].Load3(address)) * barycentrics[i];
	//	address += 12;
	//
	//	// Load and interpolate tangent
	//	address += 12;
	//
	//	// Load bitangent direction
	//	address += 4;
	//
	//	// Load and interpolate texture coordinates
	//	v.uv += asfloat(vertices[geometry_id].Load2(address)) * barycentrics[i];
	//}

	//// Transform normal from local to world space
	//v.normal = normalize(mul(ObjectToWorld3x4(), float4(v.normal, 0.0f)).xyz);

	//// Calculate geometry normal from triangle vertices positions
	//float3 edge20 = triangle_vertices[2] - triangle_vertices[0];
	//float3 edge21 = triangle_vertices[2] - triangle_vertices[1];
	//float3 edge10 = triangle_vertices[1] - triangle_vertices[0];

	//v.geometryNormal = normalize(cross(edge20, edge10));

	return v;
}

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	uint primitive_index = PrimitiveIndex();
	
	float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

	uint material_id;
	uint geometry_id;
	UnpackInstanceID(primitive_index, material_id, geometry_id);
	
	VertexData vertex_data = GetVertexData(geometry_id, primitive_index, barycentrics);

	const float3 A = float3(1, 0, 0);
	const float3 B = float3(0, 1, 0);
	const float3 C = float3(0, 0, 1);

	float3 hit_color = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;

	payload.color_distance = float4(hit_color, RayTCurrent());
}
