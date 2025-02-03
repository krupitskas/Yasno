struct PixelShaderInput
{
	float4 Position			: SV_Position;
	float3 LocalPosition	: POSITION;
	float2 TexCoord			: TEXCOORD;
};

Texture2D g_texture : register(t0);
SamplerState g_linear_sampler : register(s0);

//float2 EquirectangularToCubemap(float3 v)
//{
//    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
//    uv *= float2(0.1591, 0.3183); // 1/(2*PI), 1/PI
//    uv += 0.5;
//    return uv;
//}

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

    //if(IN.LocalPosition.x > 0.99)
    //{
    //    return float4(1.0, 0.0, 0.0, 1.0);
    //}
    //else if(IN.LocalPosition.x < -0.99)
    //{
    //    return float4(1.0, 0.0, 1.0, 1.0);
    //}
    //else
    //{
    //    if(IN.LocalPosition.y > 0.99)
    //    {
    //        return float4(0.0, 1.0, 0.0, 1.0);
    //    }
    //    else if(IN.LocalPosition.y < -0.99)
    //    {
    //        return float4(1.0, 1.0, 0.0, 1.0);
    //    }
    //    else
    //    {
    //        if(IN.LocalPosition.z > 0.99)
    //        {
    //            return float4(0.0, 0.0, 1.0, 1.0);
    //        }
    //        else if(IN.LocalPosition.z < -0.99)
    //        {
    //            return float4(0.0, 1.0, 1.0, 1.0);
    //        }
    //    }
    //}

    //return float4(1.0, 1.0, 1.0, 1.0);
}
