//#include "Common.hlsl"

// Raytracing helpers below

// Hit information, aka ray payload
// This sample only carries a shading color and hit distance.
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
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
	int vertices_before;
	int indices_before;
	int pad;
};
StructuredBuffer<PerInstanceData> PerInstanceBuffer : register(t4);

uint3 GetIndices(uint geometry_id, uint triangle_index)
{
	PerInstanceData data = PerInstanceBuffer[geometry_id];

	int address = data.indices_before + triangle_index * 3;

	uint i0 = IndexBuffer[address + 0];
	uint i1 = IndexBuffer[address + 1];
	uint i2 = IndexBuffer[address + 2];

	return uint3(i0, i1, i2);
}

VertexData GetVertexData(uint geometry_id, uint triangle_index, float3 barycentrics)
{
	// Get the triangle indices
	uint3 indices = GetIndices(geometry_id, triangle_index);

	VertexData v = (VertexData)0;

	VertexLayout input_vertex[3];

	float3 triangle_vertices[3];

	int address = PerInstanceBuffer[geometry_id].vertices_before;

	// Interpolate the vertex attributes
	for (uint i = 0; i < 3; i++)
	{
		input_vertex[i] = VertexBuffer[address + indices[i]];

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
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

	VertexData vertex = GetVertexData(InstanceIndex(), PrimitiveIndex(), barycentrics);

	payload.encoded_normals = EncodeNormals(vertex.geometry_normal, vertex.shading_normal);
	payload.hit_position = vertex.position;
	payload.material_id = PerInstanceBuffer[InstanceIndex()].material_id;
	payload.uvs = vertex.uv;
}
