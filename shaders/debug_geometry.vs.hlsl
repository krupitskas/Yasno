#include "shader_structs.h"

ConstantBuffer<CameraParameters> camera : register(b0);

struct DebugRenderVertex
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VertexShaderOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

VertexShaderOutput main(DebugRenderVertex INPUT)
{
    VertexShaderOutput OUTPUT;

    OUTPUT.position = mul(camera.view_projection, float4(INPUT.position, 1.0));
    OUTPUT.color = INPUT.color;

    return OUTPUT;
}
