struct RS2PS
{
	float4 position : SV_POSITION;

	float4 position_shadow_space : POSITION;

#ifdef HAS_NORMAL
	float3 normal : NORMAL;
#endif

#ifdef HAS_TANGENT
	float4 tangent : TANGENT;
#endif

#ifdef HAS_TEXCOORD_0
	float2 texcoord_0: TEXCOORD_0;
#endif

#ifdef HAS_TEXCOORD_1
	float2 texcoord_1: TEXCOORD_1;
#endif
};

struct PBRMetallicRoughness
{
	float4 baseColorFactor;
	int baseColorTextureIndex;
	int baseColorSamplerIndex;
	float metallicFactor;
	float roughnessFactor;
	int metallicRoughnessTextureIndex;
	int metallicRoughnessSamplerIndex;
	int normalTextureIndex;
	int normalSamplerIndex;
	int occlusionTextureIndex;
	int occlusionSamplerIndex;
	int emissiveTextureIndex;
	int emissiveSamplerIndex;

	int basecolor_indirect_index;
};

cbuffer CameraParameters : register(b0)
{
	float4x4 view_projection;
	float4x4 view;
	float4x4 projection;
	float4x4 view_inverse;
	float4x4 projection_inverse;
	float3 camera_position;
};

cbuffer Material : register(b2)
{
	PBRMetallicRoughness pbrMetallicRoughness;
};

cbuffer SceneParameters : register(b3)
{
	float4x4	shadow_matrix;
	float4		directional_light_color;
	float4		directional_light_direction;
	float		directional_light_intensity;
	float		ambient_light_intensity;
	uint		shadows_enabled;
};

Texture2D textures[5]				: register(t0);
SamplerState MaterialSamplers[5]	: register(s0);

SamplerState ShadowSampler			: register(s5);

Texture2D shadow_map				: register(t6);

float ShadowCalculation(float4 position)
{
	float3 projected_coordinate;
	projected_coordinate.x = position.x / position.w * 0.5 + 0.5;
	projected_coordinate.y = -position.y / position.w * 0.5 + 0.5;
	projected_coordinate.z = position.z / position.w;

	float closest_depth = shadow_map.Sample(ShadowSampler, projected_coordinate.xy);

	float current_depth = projected_coordinate.z + 0.005;

	float in_shadow = current_depth > closest_depth ? 1.0 : 0.0;  

	return in_shadow;
}

// Lambertian
float3 diffuse(float3 albedo, float3 lightColor, float NdotL)
{
	return albedo * (lightColor * max(NdotL, 0.0f));
}

float3 fresnel(float m, float3 lightColor, float3 F0, float NdotH, float NdotL, float LdotH)
{
	float3 R = F0 + (1.0f - F0) * (1.0f - max(LdotH, 0.0f));
	R = R * ((m + 8) / 8) * pow(NdotH, m);
	return saturate(lightColor * max(NdotL, 0.0f) * R);
}

float4 main(RS2PS input) : SV_Target
{
	float2 uv = float2(0.0, 0.0);
	float3 normal = 0;
	float4 tangent = 0;

#ifdef HAS_TEXCOORD_0
	uv = input.texcoord_0;
#endif

#ifdef HAS_NORMAL
	normal = input.normal;
#endif

#ifdef HAS_TANGENT
	tangent = input.tangent;
#endif

	float4 baseColor = pbrMetallicRoughness.baseColorFactor;
	float metallicResult = pbrMetallicRoughness.metallicFactor;
	float roughnessResult = pbrMetallicRoughness.roughnessFactor;
	float3 normals = 1;
	float occlusion = 0;
	float3 emissive = 0;

#ifdef HAS_TEXCOORD_0

	if (pbrMetallicRoughness.baseColorTextureIndex >= 0)
	{
		//baseColor *= textures[pbrMetallicRoughness.baseColorTextureIndex].Sample(MaterialSamplers[pbrMetallicRoughness.baseColorSamplerIndex], uv);
		Texture2D base_color_texture = ResourceDescriptorHeap[pbrMetallicRoughness.basecolor_indirect_index];
		baseColor = base_color_texture.Sample(MaterialSamplers[pbrMetallicRoughness.baseColorSamplerIndex], uv);

		if(baseColor.a < 0.5)
		{
		//	discard;
		}
	}

	if (pbrMetallicRoughness.metallicRoughnessTextureIndex >= 0)
	{
		float4 metallicRoughnessResult = textures[pbrMetallicRoughness.metallicRoughnessTextureIndex].Sample(MaterialSamplers[pbrMetallicRoughness.metallicRoughnessSamplerIndex], uv);

		metallicResult *= metallicRoughnessResult.b;
		roughnessResult *= metallicRoughnessResult.g;
	}

	if (pbrMetallicRoughness.normalTextureIndex >= 0)
	{
		normals = textures[pbrMetallicRoughness.normalTextureIndex].Sample(MaterialSamplers[pbrMetallicRoughness.normalSamplerIndex], uv);
		normals = 2.0f * normals - 1.0f;                                              // [0,1] ->[-1,1]
	}

	if (pbrMetallicRoughness.occlusionTextureIndex >= 0)
	{
		occlusion = textures[pbrMetallicRoughness.occlusionTextureIndex].Sample(MaterialSamplers[pbrMetallicRoughness.occlusionSamplerIndex], uv);
	}

	if (pbrMetallicRoughness.emissiveTextureIndex >= 0)
	{
		emissive = textures[pbrMetallicRoughness.emissiveTextureIndex].Sample(MaterialSamplers[pbrMetallicRoughness.emissiveSamplerIndex], uv).rgb;
	}

#endif // HAS_TEXCOORD_0

	float3 V = normalize(camera_position.xyz - input.position.xyz); // From the shading location to the camera
	float3 L = directional_light_direction.xyz; //normalize(LightPosition.xyz - input.position.xyz); // From the shading location to the LIGHT
	float3 N = normal; // Interpolated vertex normal

	float NdotL = dot(N, L);

	float3 dieletricSpecular = { 0.04f, 0.04f, 0.04f };
	float3 black = { 0.0f, 0.0f, 0.0f };
	float3 C_ambient = ambient_light_intensity * baseColor;// * occlusion.x;
	float3 C_diff = lerp(baseColor.rgb * (1.0f - dieletricSpecular.r), black, metallicResult);
	float3 C_diffuse = diffuse(C_diff, directional_light_color.rgb * directional_light_intensity, NdotL);

	const float in_shadow = shadows_enabled > 0 ? ShadowCalculation(input.position_shadow_space) : 1.0;

	float3 f = C_ambient + C_diffuse * in_shadow + emissive.xyz; //  + cubeMapSample.xyz + Specular

	return float4(f.xyz, 1.0);
}
