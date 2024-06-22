struct RS2PS
{
	float4 position : SV_POSITION;
	float4 position_shadow_space : POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 texcoord_0: TEXCOORD_0;
};

#define ALBEDO_ENABLED_BITMASK				0
#define METALLIC_ROUGHNESS_ENABLED_BITMASK	1
#define NORMAL_ENABLED_BITMASK				2
#define OCCLUSION_ENABLED_BITMASK			3
#define EMISSIVE_ENABLED_BITMASK			4

struct SurfaceShaderParameters
{
	float4 base_color_factor;
	float metallic_factor;
	float roughness_factor;

	int texture_enable_bitmask; // Encoded which textures are should be used

	int albedo_texture_index;
	int metallic_roughness_texture_index;
	int normal_texture_index;
	int occlusion_texture_index;
	int emissive_texture_index;
};

// Input buffers
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

cbuffer SceneParameters : register(b1)
{
	float4x4	shadow_matrix;
	float4		directional_light_color;
	float4		directional_light_direction;
	float		directional_light_intensity;
	float		ambient_light_intensity;
	uint		shadows_enabled;
};

cbuffer InstanceId : register(b2)
{
    uint instance_id;
};

struct PerInstanceData
{
	float4x4 model_matrix;
	int material_id;
	int vertices_before;
	int indices_before;
	int pad;
};
StructuredBuffer<PerInstanceData> per_instance_data : register(t0);

// Global material buffer
StructuredBuffer<SurfaceShaderParameters> materials_buffer : register(t1);

// Input textures
Texture2D ShadowMap					: register(t2);

// Samplers
SamplerState ShadowSampler			: register(s0);
SamplerState LinearSampler			: register(s1);

float ShadowCalculation(float4 position)
{
	float3 projected_coordinate;
	projected_coordinate.x = position.x / position.w * 0.5 + 0.5;
	projected_coordinate.y = -position.y / position.w * 0.5 + 0.5;
	projected_coordinate.z = position.z / position.w;

	float closest_depth = ShadowMap.Sample(ShadowSampler, projected_coordinate.xy);

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

	uv = input.texcoord_0;
	normal = input.normal;
	tangent = input.tangent;

	PerInstanceData instance_data = per_instance_data[instance_id];
	
	SurfaceShaderParameters pbr_material = materials_buffer[instance_data.material_id];

	float4 base_color = pbr_material.base_color_factor;
	float metallicResult = pbr_material.metallic_factor;
	float roughnessResult = pbr_material.roughness_factor;
	float3 normals = 1;
	float occlusion = 0;
	float3 emissive = 0;

	if (pbr_material.texture_enable_bitmask & (1 << ALBEDO_ENABLED_BITMASK))
	{
		Texture2D albedo_texture = ResourceDescriptorHeap[pbr_material.albedo_texture_index];
		base_color *= albedo_texture.Sample(LinearSampler, uv);

		if(base_color.a < 0.5)
		{
			discard;
		}
	}

	if (pbr_material.texture_enable_bitmask & (1 << METALLIC_ROUGHNESS_ENABLED_BITMASK))
	{
		Texture2D metallic_roughness_texture = ResourceDescriptorHeap[pbr_material.metallic_roughness_texture_index];

		float4 metallicRoughnessResult = metallic_roughness_texture.Sample(LinearSampler, uv);

		metallicResult *= metallicRoughnessResult.b;
		roughnessResult *= metallicRoughnessResult.g;
	}

	if (pbr_material.texture_enable_bitmask & (1 << NORMAL_ENABLED_BITMASK))
	{
		Texture2D normal_texture = ResourceDescriptorHeap[pbr_material.normal_texture_index];

		normals = normal_texture.Sample(LinearSampler, uv);
		normals = 2.0f * normals - 1.0f; // [0,1] ->[-1,1]
	}

	if (pbr_material.texture_enable_bitmask & (1 << OCCLUSION_ENABLED_BITMASK))
	{
		Texture2D occlusion_texture = ResourceDescriptorHeap[pbr_material.occlusion_texture_index];

		occlusion = occlusion_texture.Sample(LinearSampler, uv);
	}

	if (pbr_material.texture_enable_bitmask & (1 << EMISSIVE_ENABLED_BITMASK))
	{
		Texture2D emissive_texture = ResourceDescriptorHeap[pbr_material.emissive_texture_index];

		emissive = emissive_texture.Sample(LinearSampler, uv).rgb;
	}

	float3 V = normalize(camera_position.xyz - input.position.xyz); // From the shading location to the camera
	float3 L = directional_light_direction.xyz; //normalize(LightPosition.xyz - input.position.xyz); // From the shading location to the LIGHT
	float3 N = normal; // Interpolated vertex normal

	float NdotL = dot(N, L);

	float3 dieletricSpecular = { 0.04f, 0.04f, 0.04f };
	float3 black = { 0.0f, 0.0f, 0.0f };
	float3 C_ambient = ambient_light_intensity * base_color;// * occlusion.x;
	float3 C_diff = lerp(base_color.rgb * (1.0f - dieletricSpecular.r), black, metallicResult);
	float3 C_diffuse = diffuse(C_diff, directional_light_color.rgb * directional_light_intensity, NdotL);

	const float in_shadow = shadows_enabled > 0 ? ShadowCalculation(input.position_shadow_space) : 1.0;

	float3 f = C_ambient + C_diffuse * in_shadow + emissive.xyz; //  + cubeMapSample.xyz + Specular

	return float4(f.xyz, 1.0);
}
