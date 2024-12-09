module;

#include <DirectXMath.h>

export module graphics.lights;

// TODO: Use LUXes here instead of "intensities"

export namespace ysn
{
struct EnvironmentLight
{
    float intensity = 0.25f;
};

struct DirectionalLight
{
    DirectX::XMFLOAT4 color = {1.0f, 1.0, 1.0f, 0.0f};
    DirectX::XMFLOAT4 direction = {0.0f, 1.0, 0.6f, 0.0f};
    float intensity = 100.0f;

    bool cast_shadow = true;
};

struct PointLight
{
    float intensity = 0.0f;
    DirectX::XMFLOAT3 color = {0.0f, 0.0, 0.0f};
};

struct SpotLight
{
    float intensity = 0.0f;
    DirectX::XMFLOAT3 color = {0.0f, 0.0, 0.0f};
};
} // namespace ysn
