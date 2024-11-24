#include "CameraController.hpp"

#include <DirectXMath.h>
#include <print>

namespace
{
	DirectX::XMFLOAT3 MoveWithDirection(const float Distance, DirectX::XMFLOAT3 Direction)
	{
		Direction.x *= Distance;
		Direction.y *= Distance;
		Direction.z *= Distance;

		return Direction;
	}
} // namespace

namespace ysn
{
	bool CameraController::IsMoved()
	{
		return pCamera->IsMoved();
	}

	void CameraController::Enable()
	{
		m_IsActive = true;
	}

	void CameraController::Disable()
	{
		m_IsActive = false;
	}

	void FpsCameraController::MoveLeft(float Distance) const
	{
		if (!m_IsActive)
			return;

		if (m_IsMovementBoostActive)
		{
			Distance *= m_MovementBoost;
		}

		Distance *= mouse_speed;

		pCamera->Move(MoveWithDirection(Distance, pCamera->GetRightVector()));
	}

	void FpsCameraController::MoveRight(float Distance) const
	{
		if (!m_IsActive)
			return;

		if (m_IsMovementBoostActive)
		{
			Distance *= m_MovementBoost;
		}

		Distance *= mouse_speed;

		pCamera->Move(MoveWithDirection(-Distance, pCamera->GetRightVector()));
	}

	void FpsCameraController::MoveForward(float Distance) const
	{
		if (!m_IsActive)
			return;

		if (m_IsMovementBoostActive)
		{
			Distance *= m_MovementBoost;
		}

		Distance *= mouse_speed;

		pCamera->Move(MoveWithDirection(Distance, pCamera->GetForwardVector()));
	}

	void FpsCameraController::MoveBackwards(float Distance) const
	{
		if (!m_IsActive)
			return;

		if (m_IsMovementBoostActive)
		{
			Distance *= m_MovementBoost;
		}

		Distance *= mouse_speed;

		pCamera->Move(MoveWithDirection(-Distance, pCamera->GetForwardVector()));
	}

	void FpsCameraController::MoveUp(float Distance) const
	{
		if (!m_IsActive)
			return;

		if (m_IsMovementBoostActive)
		{
			Distance *= m_MovementBoost;
		}

		Distance *= mouse_speed;

		pCamera->Move(MoveWithDirection(Distance, pCamera->GetUpVector()));
	}

	void FpsCameraController::MoveDown(float Distance) const
	{
		if (!m_IsActive)
			return;

		if (m_IsMovementBoostActive)
		{
			Distance *= m_MovementBoost;
		}

		Distance *= mouse_speed;

		pCamera->Move(MoveWithDirection(-Distance, pCamera->GetUpVector()));
	}

	void FpsCameraController::MoveMouse(const int MousePositionX, const int MousePositionY)
	{
		if (!m_IsActive)
			return;

		// limit pitch to straight up or straight down
		// constexpr float Limit = DirectX::XM_PIDIV2 - 0.01f;
		// m_Pitch = std::max(-Limit, m_Pitch);
		// m_Pitch = std::min(+Limit, m_Pitch);

		// keep longitude in sane range by wrapping
		// if (m_Yaw > DirectX::XM_PI)
		// {
		// 	m_Yaw -= DirectX::XM_2PI;
		// }
		// else if (m_Yaw < -DirectX::XM_PI)
		// {
		// 	m_Yaw += DirectX::XM_2PI;
		// }

		m_Yaw -= MousePositionX * m_MouseSensitivity;
		m_Pitch -= MousePositionY * m_MouseSensitivity;

		pCamera->SetYaw(m_Yaw);
		pCamera->SetPitch(m_Pitch);
	}
} // namespace ysn
