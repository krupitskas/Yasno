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

    OUTPUT.position = mul(view_projection, float4(INPUT.position, 1.0));
    OUTPUT.color = INPUT.color;

    return OUTPUT;
}
