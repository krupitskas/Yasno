#if __cplusplus
#pragma once
#include <DirectXMath.h>
#include <d3d12.h>

import system.math;

#define YSN_SHADER_STRUCT struct alignas(16)

// #define matrix DirectX::XMMATRIX
// #define float4 DirectX::XMFLOAT4
// #define float3 DirectX::XMFLOAT3
// #define float2 DirectX::XMFLOAT2
// #define float16_t uint16_t
// #define float16_t2 uint32_t
// #define uint uint32_t
// #define int int32_t
// #define float float
#define OUT_PARAMETER(X) X&
#endif

#define ALBEDO_ENABLED_BIT 0
#define METALLIC_ROUGHNESS_ENABLED_BIT 1
#define NORMAL_ENABLED_BIT 2
#define OCCLUSION_ENABLED_BIT 3
#define EMISSIVE_ENABLED_BIT 4

// Default PBR material
YSN_SHADER_STRUCT SurfaceShaderParameters
{
    DirectX::XMFLOAT4 base_color_factor;
    float metallic_factor = 0.0f;
    float roughness_factor = 0.0f;

    // Encods which textures are active
    int32_t texture_enable_bitmask = 0;

    int32_t albedo_texture_index = 0;
    int32_t metallic_roughness_texture_index = 0;
    int32_t normal_texture_index = 0;
    int32_t occlusion_texture_index = 0;
    int32_t emissive_texture_index = 0;
};

// Per instance data for rendering, this can be split into smaller parts
YSN_SHADER_STRUCT RenderInstanceData
{
    DirectX::XMMATRIX model_matrix;
    int32_t material_id;
    int32_t vertices_before;
    int32_t indices_before; // offset
    int32_t pad;
};

YSN_SHADER_STRUCT GpuSceneParameters
{
    DirectX::XMFLOAT4X4 shadow_matrix;
    DirectX::XMFLOAT4 directional_light_color = {0.0f, 0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 directional_light_direction = {0.0f, 0.0f, 0.0f, 0.0f};
    float directional_light_intensity = 0.0f;
    float ambient_light_intensity = 0.0f;
    uint32_t shadows_enabled = 0;

    uint32_t pad[1];

    static uint32_t GetGpuSize()
    {
        return static_cast<uint32_t>(ysn::AlignPow2(sizeof(GpuSceneParameters), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
    }
};

YSN_SHADER_STRUCT ShadowCamera
{
    DirectX::XMFLOAT4X4 shadow_matrix;
};

YSN_SHADER_STRUCT TonemapParameters
{
    uint32_t display_width = 0;
    uint32_t display_height = 0;
    uint32_t tonemap_method = 0;
    float exposure = 0.0f;

    static uint32_t GetGpuSize()
    {
        return static_cast<uint32_t>(ysn::AlignPow2(sizeof(TonemapParameters), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
    }
};

YSN_SHADER_STRUCT GenerateMipsConstantBuffer
{
    uint32_t src_mip_level;       // Texture level of source mip
    uint32_t num_mip_levels;      // Number of OutMips to write: [1-4]
    uint32_t src_dimension;       // Width and height of the source texture are even or odd.
    uint32_t is_srgb;             // Must apply gamma correction to sRGB textures.
    DirectX::XMFLOAT2 texel_size; // 1.0 / OutMip1.Dimensions

    uint32_t pad[2];
};

YSN_SHADER_STRUCT GpuCamera
{
    DirectX::XMFLOAT4X4 view_projection;
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 projection;
    DirectX::XMFLOAT4X4 view_inverse;
    DirectX::XMFLOAT4X4 projection_inverse;
    DirectX::XMFLOAT3 position;
    uint32_t frame_number;
    uint32_t frames_accumulated;
    uint32_t reset_accumulation;
    uint32_t accumulation_enabled;
    uint32_t pad;

    static uint32_t GetGpuSize()
    {
        return static_cast<uint32_t>(ysn::AlignPow2(sizeof(GpuCamera), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
    }
};

#if __cplusplus
#undef matrix
#undef float4
#undef float3
#undef float2
#undef uint
#undef float16_t
#undef float16_t2
#undef OUT_PARAMETER
#endif
