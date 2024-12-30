// TODO: Move to shader_structs.h
struct DebugLinePoint
{
	float3 position;
	float3 color; // TODO: compress to uint
};

RWStructuredBuffer<DebugLinePoint>	g_line_vertex_buffer	: register(u126);
RWByteAddressBuffer					g_line_args_buffer		: register(u127);

void DrawLine(float3 position0, float3 position1, float3 color)
{
	uint offset_in_vertex_buffer;
	g_line_args_buffer.InterlockedAdd(0, 1, offset_in_vertex_buffer);
	offset_in_vertex_buffer = offset_in_vertex_buffer * 2;

	DebugLinePoint p;
	p.position = position0;
	p.color = color;

	g_line_vertex_buffer[offset_in_vertex_buffer + 0] = p;

	p.position = position1;
	p.color = color;

	g_line_vertex_buffer[offset_in_vertex_buffer + 1] = p;
}

void DrawAxisAlignedBox(float3 center, float3 extent, float3 color)
{
	// Calculate the 8 corners of the AABB
    float3 corners[8];

    corners[0] = center + float3(-extent.x, -extent.y, -extent.z); // min corner
    corners[1] = center + float3( extent.x, -extent.y, -extent.z);
    corners[2] = center + float3(-extent.x,  extent.y, -extent.z);
    corners[3] = center + float3( extent.x,  extent.y, -extent.z);
    corners[4] = center + float3(-extent.x, -extent.y,  extent.z);
    corners[5] = center + float3( extent.x, -extent.y,  extent.z);
    corners[6] = center + float3(-extent.x,  extent.y,  extent.z);
    corners[7] = center + float3( extent.x,  extent.y,  extent.z);

    // Draw edges of the AABB
    // Bottom face
    DrawLine(corners[0], corners[1], color);
    DrawLine(corners[1], corners[3], color);
    DrawLine(corners[3], corners[2], color);
    DrawLine(corners[2], corners[0], color);

    // Top face
    DrawLine(corners[4], corners[5], color);
    DrawLine(corners[5], corners[7], color);
    DrawLine(corners[7], corners[6], color);
    DrawLine(corners[6], corners[4], color);

    // Vertical edges
    DrawLine(corners[0], corners[4], color);
    DrawLine(corners[1], corners[5], color);
    DrawLine(corners[2], corners[6], color);
    DrawLine(corners[3], corners[7], color);
}

void DrawSphere(float3 center, float radius, float3 color)
{
    const int slices = 16; // Number of slices along latitude
    const int stacks = 16; // Number of stacks along longitude

    for (int i = 0; i < stacks; ++i)
    {
        float theta1 = (i / (float)stacks) * 3.14159265; // Current stack angle
        float theta2 = ((i + 1) / (float)stacks) * 3.14159265; // Next stack angle

        for (int j = 0; j < slices; ++j)
        {
            float phi1 = (j / (float)slices) * 2.0 * 3.14159265; // Current slice angle
            float phi2 = ((j + 1) / (float)slices) * 2.0 * 3.14159265; // Next slice angle

            // Calculate vertices of the current quad
            float3 p1 = center + radius * float3(sin(theta1) * cos(phi1), cos(theta1), sin(theta1) * sin(phi1));
            float3 p2 = center + radius * float3(sin(theta2) * cos(phi1), cos(theta2), sin(theta2) * sin(phi1));
            float3 p3 = center + radius * float3(sin(theta2) * cos(phi2), cos(theta2), sin(theta2) * sin(phi2));
            float3 p4 = center + radius * float3(sin(theta1) * cos(phi2), cos(theta1), sin(theta1) * sin(phi2));

            // Draw lines for the current quad
            DrawLine(p1, p2, color);
            DrawLine(p2, p3, color);
            DrawLine(p3, p4, color);
            DrawLine(p4, p1, color);
        }
    }
}
