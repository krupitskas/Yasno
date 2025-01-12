struct PixelShaderInput
{
	float2 TexCoord : TEXCOORD;
	float4 Position : SV_Position;
	float3 LocalPosition : POSITION;
};

Texture2D g_texture : register(t0);
SamplerState g_linear_sampler : register(s0);

float2 EquirectangularToCubemap(float3 direction)
{
    // Normalize the direction vector
    direction = normalize(direction);

    // Calculate spherical coordinates from the adjusted direction
    float theta = acos(direction.y);          // Inclination (from +Y axis)
    float phi = atan2(direction.x, direction.z); // Azimuth (angle in XZ-plane)

    // Normalize phi to [0, 2π] and map to [0, 1] for texture coordinates
    if (phi < 0.0) phi += 2.0 * 3.14159265359;
    float u = phi / (2.0 * 3.14159265359);

    // Map theta to [0, 1] for texture coordinates
    float v = theta / 3.14159265359;

    return float2(u, v);
}

float4 main(PixelShaderInput IN) : SV_Target
{
    float2 uv = EquirectangularToCubemap(normalize(IN.LocalPosition)); 
    return g_texture.Sample(g_linear_sampler, uv);
}
