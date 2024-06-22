#include "Camera.hpp"

#include <d3d12.h>

#include <SimpleMath.h>

namespace ysn
{
	using namespace DirectX;
	using namespace DirectX::SimpleMath;

	uint32_t GpuCamera::GetGpuSize()
	{
		return static_cast<uint32_t>(ysn::AlignPow2(sizeof(GpuCamera), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
	}

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
		const XMVECTOR FocusPosition = XMVectorSet(m_Position.x + m_ForwardVector.x, m_Position.y + m_ForwardVector.y, m_Position.z + m_ForwardVector.z, 0.0);

		const XMVECTOR RightVector = XMVector3Cross(XMLoadFloat3(&Vector3::Up), XMLoadFloat3(&m_ForwardVector));

		m_ViewMatrix = XMMatrixLookAtRH(Position, FocusPosition, Vector3::Up);
		m_ProjectionMatrix = XMMatrixPerspectiveFovRH(XMConvertToRadians(fov), m_AspectRatio, m_NearPlane, m_FarPlane);

		XMStoreFloat3(&m_RightVector, RightVector);
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
	}

	void Camera::Move(const XMFLOAT3 Position)
	{
		m_Position.x += Position.x;
		m_Position.y += Position.y;
		m_Position.z += Position.z;
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
	}

	void Camera::SetPitch(const float Pitch)
	{
		m_Pitch = Pitch;
	}
} // namespace ysn
