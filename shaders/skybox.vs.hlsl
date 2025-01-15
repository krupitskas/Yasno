#include "debug_renderer.hlsl"
#include "shader_structs.h"
#include "shared.hlsl"

ConstantBuffer<CameraParameters> parameters : register(b0);

struct VertexShaderOutput
{
    float4 Position             : SV_Position;
    float3 OriginalPosition     : POSITION;
    float2 TexCoord             : TEXCOORD;
};

VertexShaderOutput main(VertexLayout input)
{
    VertexShaderOutput output;

    float4x4 rotation_view = parameters.view;
    rotation_view[0][3] = 0;
    rotation_view[1][3] = 0;
    rotation_view[2][3] = 0;
    float4 clip_pos = mul(parameters.projection, mul(rotation_view, float4(input.position, 1.0)));
    
    output.Position = clip_pos.xyww;
    output.OriginalPosition = input.position;
    output.TexCoord = input.texcoord_0;

    //DrawAxisAlignedBox(input.position, float3(1.0, 1.0, 1.0), float3(255,0,0));
    //DrawSphere(input.position - input.normal.xyz * 10, 1.0, float3(255,0,0));
    //DrawLine(input.position.xyz, input.position.xyz + input.normal.xyz, float3(255,0,0));

    return output;
}
