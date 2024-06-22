#pragma once

#include "Camera.hpp"

#include <memory>

namespace ysn
{
	class CameraController
	{
	public:
		std::shared_ptr<Camera> pCamera;

		void Enable();
		void Disable();

	protected:
		bool m_IsActive = true;
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

		bool m_IsMovementBoostActive = false;

		float mouse_speed = 50.0f;
	private:
		float m_Yaw = 0.0f;
		float m_Pitch = 0.0f;
		float m_MovementBoost = 5.0;
		float m_MouseSensitivity = 0.1f;
	};
}
