#include "debug_renderer.hlsl"
#include "shader_structs.h"

ConstantBuffer<CameraParameters> parameters : register(b0);

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

    float4x4 rotation_view = parameters.view;
    rotation_view[0][3] = 0;
    rotation_view[1][3] = 0;
    rotation_view[2][3] = 0;
    float4 clip_pos = mul(parameters.projection, mul(rotation_view, float4(input.Position, 1.0)));
    
    output.Position = clip_pos.xyww;
    output.OriginalPosition = input.Position;
    output.TexCoord = input.TexCoord;

    //DrawAxisAlignedBox(camera_position, float3(1.0, 1.0, 1.0), float3(255,0,0));
    //DrawSphere(camera_position, 1.0, float3(255,0,0));

    return output;
}
