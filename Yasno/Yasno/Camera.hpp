#pragma once

#include <DirectXMath.h>

namespace ysn
{
    YSN_SHADER_STRUCT GpuCamera
    {
        DirectX::XMFLOAT4X4 view_projection;
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 projection;
        DirectX::XMFLOAT4X4 view_inverse;
        DirectX::XMFLOAT4X4 projection_inverse;
        DirectX::XMFLOAT3	position;
        uint32_t			pad;

        static uint32_t GetGpuSize();
    };

    struct Camera
    {
        Camera();

        DirectX::XMMATRIX GetViewMatrix() const;
        DirectX::XMMATRIX GetProjectionMatrix() const;

        void Update();
        void SetAspectRatio(float AspectRatio);

        float GetFov() const;
        void SetFov(float Fov);

        DirectX::XMFLOAT3 GetPosition() const;
        void SetPosition(DirectX::XMFLOAT3 Position);
        void Move(DirectX::XMFLOAT3 Position);

        // Right handed coodrinate space
        DirectX::XMFLOAT3 GetForwardVector() const; // Z forward
        DirectX::XMFLOAT3 GetRightVector() const; // -X left
        DirectX::XMFLOAT3 GetUpVector() const; // Y up

        void SetYaw(float Yaw);
        void SetPitch(float Pitch);

        float fov = 45.0f;
    private:
        float m_AspectRatio = 1.0f;
        float m_NearPlane = 0.1f;
        float m_FarPlane = 10000.f;

        float m_Pitch = 0.0f; // Vertical rotation
        float m_Yaw = 0.0f; // Horizontal rotation

        DirectX::XMFLOAT3 m_Position;

        DirectX::XMFLOAT3 m_ForwardVector;
        DirectX::XMFLOAT3 m_RightVector;

        DirectX::XMMATRIX m_ViewMatrix;
        DirectX::XMMATRIX m_ProjectionMatrix;

        float m_MouseSensitivity = 1.0f;
    };
}
