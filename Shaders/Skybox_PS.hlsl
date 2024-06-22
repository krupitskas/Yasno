// #version 330 core
// out vec4 FragColor;
//
// in vec3 localPos;
//   
// uniform samplerCube environmentMap;
//   
// void main()
// {
//     vec3 envColor = texture(environmentMap, localPos).rgb;
//     
//     envColor = envColor / (envColor + vec3(1.0));
//     envColor = pow(envColor, vec3(1.0/2.2)); 
//   
//     FragColor = vec4(envColor, 1.0);
// }

struct PixelShaderInput
{
    float2 TexCoord : TEXCOORD;
    float3 OriginalPosition: POSITION;
};

Texture2D g_texture : register(t0);
SamplerState g_linear_sampler : register(s0);

// Converts a direction vector to spherical coordinates
float2 SphericalCoords(float3 dir)
{
    float PI = 3.1415926535897932385;
    float theta = atan2(dir.z, dir.x);
    float phi = acos(dir.y);
    return float2((theta + PI) / (2.0f * PI), phi / PI);
}

float4 main(PixelShaderInput IN) : SV_Target
{
    float2 uv = SphericalCoords(normalize(IN.OriginalPosition)); 
    return g_texture.Sample(g_linear_sampler, uv);
    //return float4(255, 0, 0, 0);
}
