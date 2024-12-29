struct PixelShaderInput
{
    float4 position : SV_position;
    float3 color : COLOR;
};

float4 main(PixelShaderInput INPUT) : SV_Target
{
    return float4(INPUT.color, 1.0);
}
