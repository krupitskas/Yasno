#include "debug_renderer.hlsl"

cbuffer CameraParameters : register(b0)
{
    float4x4 view_projection;
    float4x4 view;
    float4x4 projection;
    float4x4 view_inverse;
    float4x4 projection_inverse;
    float3 camera_position;
    uint frame_number;
    uint frames_accumulated;
    uint reset_accumulation;
    uint accumulation_enabled;
};

struct VertexPosTexCoord
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD;
};

struct VertexShaderOutput
{
    float2 TexCoord : TEXCOORD;
    float4 Position : SV_Position;
    float3 OriginalPosition : POSITION;
};

VertexShaderOutput main(VertexPosTexCoord input)
{
    VertexShaderOutput output;

    float4x4 rotation_view = view;
    rotation_view[0][3] = 0;
    rotation_view[1][3] = 0;
    rotation_view[2][3] = 0;
    float4 clip_pos = mul(projection, mul(rotation_view, float4(input.Position, 1.0)));
    
    output.Position = clip_pos.xyww;
    output.OriginalPosition = input.Position;
    output.TexCoord = input.TexCoord;

    DrawLine(float3(0,0,0), input.Position, float3(255,0,0), float3(255,0,0));

    return output;
}
