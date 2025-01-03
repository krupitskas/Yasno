module;

#include <DirectXMath.h>

export module yasno.camera_controller;

import std;
import yasno.camera;

export namespace ysn
{
class CameraController
{
public:
    std::shared_ptr<Camera> p_camera;

    bool IsMoved();

    void Enable();
    void Disable();

protected:
    bool m_is_active = true;
};

class FpsCameraController : public CameraController
{
public:
    void MoveLeft(float Distance) const;
    void MoveRight(float Distance) const;
    void MoveForward(float Distance) const;
    void MoveBackwards(float Distance) const;
    void MoveUp(float Distance) const;
    void MoveDown(float Distance) const;

    void MoveMouse(int MousePositionX, int MousePositionY);

    bool m_is_boost_active = false;

    float mouse_speed = 5.0f;

private:
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_movement_boost = 5.0;
    float m_mouse_sensetivity = 0.1f;
};
} // namespace ysn

module :private;

DirectX::XMFLOAT3 MoveWithDirection(const float Distance, DirectX::XMFLOAT3 Direction)
{
    Direction.x *= Distance;
    Direction.y *= Distance;
    Direction.z *= Distance;

    return Direction;
}

namespace ysn
{
bool CameraController::IsMoved()
{
    return p_camera->IsMoved();
}

void CameraController::Enable()
{
    m_is_active = true;
}

void CameraController::Disable()
{
    m_is_active = false;
}

void FpsCameraController::MoveLeft(float Distance) const
{
    if (!m_is_active)
        return;

    if (m_is_boost_active)
    {
        Distance *= m_movement_boost;
    }

    Distance *= mouse_speed;

    p_camera->Move(MoveWithDirection(Distance, p_camera->GetRightVector()));
}

void FpsCameraController::MoveRight(float Distance) const
{
    if (!m_is_active)
        return;

    if (m_is_boost_active)
    {
        Distance *= m_movement_boost;
    }

    Distance *= mouse_speed;

    p_camera->Move(MoveWithDirection(-Distance, p_camera->GetRightVector()));
}

void FpsCameraController::MoveForward(float Distance) const
{
    if (!m_is_active)
        return;

    if (m_is_boost_active)
    {
        Distance *= m_movement_boost;
    }

    Distance *= mouse_speed;

    p_camera->Move(MoveWithDirection(Distance, p_camera->GetForwardVector()));
}

void FpsCameraController::MoveBackwards(float Distance) const
{
    if (!m_is_active)
        return;

    if (m_is_boost_active)
    {
        Distance *= m_movement_boost;
    }

    Distance *= mouse_speed;

    p_camera->Move(MoveWithDirection(-Distance, p_camera->GetForwardVector()));
}

void FpsCameraController::MoveUp(float Distance) const
{
    if (!m_is_active)
        return;

    if (m_is_boost_active)
    {
        Distance *= m_movement_boost;
    }

    Distance *= mouse_speed;

    p_camera->Move(MoveWithDirection(Distance, p_camera->GetUpVector()));
}

void FpsCameraController::MoveDown(float Distance) const
{
    if (!m_is_active)
        return;

    if (m_is_boost_active)
    {
        Distance *= m_movement_boost;
    }

    Distance *= mouse_speed;

    p_camera->Move(MoveWithDirection(-Distance, p_camera->GetUpVector()));
}

void FpsCameraController::MoveMouse(const int MousePositionX, const int MousePositionY)
{
    if (!m_is_active)
        return;

    // limit pitch to straight up or straight down
    // constexpr float Limit = DirectX::XM_PIDIV2 - 0.01f;
    // m_pitch = std::max(-Limit, m_pitch);
    // m_pitch = std::min(+Limit, m_pitch);

    // keep longitude in sane range by wrapping
    // if (m_yaw > DirectX::XM_PI)
    // {
    // 	m_yaw -= DirectX::XM_2PI;
    // }
    // else if (m_yaw < -DirectX::XM_PI)
    // {
    // 	m_yaw += DirectX::XM_2PI;
    // }

    m_yaw -= MousePositionX * m_mouse_sensetivity;
    m_pitch -= MousePositionY * m_mouse_sensetivity;

    p_camera->SetYaw(m_yaw);
    p_camera->SetPitch(m_pitch);
}
} // namespace ysn
