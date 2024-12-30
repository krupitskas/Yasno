module;

#include <DirectXMath.h>
#include <d3d12.h>
#include <SimpleMath.h>

export module yasno.camera;

export namespace ysn
{
using namespace DirectX;
using namespace DirectX::SimpleMath;

struct Camera
{
    Camera();

    DirectX::XMMATRIX GetViewMatrix() const;
    // TODO: Previous View Matrix
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
    DirectX::XMFLOAT3 GetRightVector() const;   // -X left
    DirectX::XMFLOAT3 GetUpVector() const;      // Y up

    void SetYaw(float Yaw);
    void SetPitch(float Pitch);

    // NOTE: Turns falls after Update() called!
    bool IsMoved();

    float fov = 45.0f;

private:
    float m_AspectRatio = 1.0f;
    float m_NearPlane = 0.1f;
    float m_FarPlane = 10000.f;

    float m_Pitch = 0.0f; // Vertical rotation
    float m_Yaw = 0.0f;   // Horizontal rotation

    DirectX::XMFLOAT3 m_Position;

    DirectX::XMFLOAT3 m_ForwardVector;
    DirectX::XMFLOAT3 m_RightVector;

    DirectX::XMMATRIX m_ViewMatrix;
    DirectX::XMMATRIX m_ProjectionMatrix;

    float m_MouseSensitivity = 1.0f;

    bool m_is_moved = false;
};

} // namespace ysn

module :private;

namespace ysn
{
Camera::Camera()
{
    m_ProjectionMatrix = XMMatrixIdentity();
    m_ViewMatrix = XMMatrixIdentity();
    m_Position = XMFLOAT3(0, 0, 0);
    m_ForwardVector = XMFLOAT3(0, 0, 1);
    m_RightVector = XMFLOAT3(1, 0, 0);
}

XMMATRIX Camera::GetViewMatrix() const
{
    return m_ViewMatrix;
}

XMMATRIX Camera::GetProjectionMatrix() const
{
    return m_ProjectionMatrix;
}

void Camera::Update()
{
    m_ForwardVector.x = XMScalarSin(XMConvertToRadians(m_Yaw)) * XMScalarCos(XMConvertToRadians(m_Pitch));
    m_ForwardVector.y = XMScalarSin(XMConvertToRadians(m_Pitch));
    m_ForwardVector.z = XMScalarCos(XMConvertToRadians(m_Yaw)) * XMScalarCos(XMConvertToRadians(m_Pitch));

    const XMVECTOR Position = XMVectorSet(m_Position.x, m_Position.y, m_Position.z, 1.0);
    const XMVECTOR FocusPosition =
        XMVectorSet(m_Position.x + m_ForwardVector.x, m_Position.y + m_ForwardVector.y, m_Position.z + m_ForwardVector.z, 0.0);

    const XMVECTOR RightVector = XMVector3Cross(XMLoadFloat3(&Vector3::Up), XMLoadFloat3(&m_ForwardVector));

    m_ViewMatrix = XMMatrixLookAtRH(Position, FocusPosition, Vector3::Up);
    m_ProjectionMatrix = XMMatrixPerspectiveFovRH(XMConvertToRadians(fov), m_AspectRatio, m_NearPlane, m_FarPlane);

    XMStoreFloat3(&m_RightVector, RightVector);

    m_is_moved = false;
}

void Camera::SetAspectRatio(const float AspectRatio)
{
    m_AspectRatio = AspectRatio;
}

XMFLOAT3 Camera::GetPosition() const
{
    return m_Position;
}

void Camera::SetPosition(const XMFLOAT3 Position)
{
    m_Position = Position;

    m_is_moved = true;
}

void Camera::Move(const XMFLOAT3 Position)
{
    m_Position.x += Position.x;
    m_Position.y += Position.y;
    m_Position.z += Position.z;

    m_is_moved = true;
}

XMFLOAT3 Camera::GetForwardVector() const
{
    return m_ForwardVector;
}

XMFLOAT3 Camera::GetRightVector() const
{
    return m_RightVector;
}

XMFLOAT3 Camera::GetUpVector() const
{
    return Vector3::Up;
}

void Camera::SetYaw(const float Yaw)
{
    m_Yaw = Yaw;

    m_is_moved = true;
}

void Camera::SetPitch(const float Pitch)
{
    m_Pitch = Pitch;

    m_is_moved = true;
}

bool Camera::IsMoved()
{
    return m_is_moved;
}
} // namespace ysn
