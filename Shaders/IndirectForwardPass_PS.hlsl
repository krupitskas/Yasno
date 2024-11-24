struct RS2PS
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float2 texcoord_0: TEXCOORD_0;
};

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

float4 main(RS2PS input) : SV_Target
{
	return float4(1.0f, 0.0f, 0.0f, 0.0f);
}
