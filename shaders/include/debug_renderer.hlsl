// TODO: Move to shader_structs.h
struct DebugLinePoint
{
	float3 position;
	float3 color; // TODO: compress to uint
};

RWStructuredBuffer<DebugLinePoint>	g_line_vertex_buffer	: register(u126);
RWByteAddressBuffer					g_line_args_buffer		: register(u127);

void DrawLine(float3 position0, float3 position1, float3 color0, float3 color1)
{
	uint offset_in_vertex_buffer;

	g_line_args_buffer.Store(0, 2);

	//g_line_args_buffer.InterlockedAdd(0, 2, offset_in_vertex_buffer);

	//uint first_point_index = offset_in_vertex_buffer / sizeof(DebugLinePoint);

	DebugLinePoint p;
	p.position = position0;
	p.color = color0;

	g_line_vertex_buffer[0] = p;

	p.position = position1;
	p.color = color1;

	g_line_vertex_buffer[1] = p;
}

//void DrawAxisAlignedBox(float3 center, float3 extent, float4 color);
//void DrawSphere(float3 center, float radius, float4 color);