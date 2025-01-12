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
		Camera(XMVECTOR position = { 0.0f, 0.0f, 0.0f, 1.0f }, XMVECTOR focus_point = { 0.0f, 0.0f, 0.0f, 1.0f });

		XMMATRIX GetViewMatrix() const;
		XMMATRIX GetPrevViewMatrix() const;
		// TODO: Previous View Matrix
		XMMATRIX GetProjectionMatrix() const;

		void Update();
		void SetAspectRatio(float AspectRatio);

		//float GetFov() const;
		//void SetFov(float Fov);

		XMFLOAT3 GetPosition() const;
		void SetPosition(XMFLOAT3 Position);
		void Move(XMFLOAT3 Position);

		// Right handed coodrinate space
		XMFLOAT3 GetForwardVector() const; // Z forward
		XMFLOAT3 GetRightVector() const;   // -X left
		XMFLOAT3 GetUpVector() const;      // Y up

		void SetYaw(float Yaw);
		void SetPitch(float Pitch);

		float GetYaw() const;
		float GetPitch() const;

		// NOTE: Become false after Update() has been called
		bool IsMoved();

		float fov = 45.0f;

	private:
		float m_aspect_ratio = 1.0f;
		float m_near_plane = 0.1f;
		float m_far_plane = 10000.f;

		float m_pitch = 0.0f; // Vertical rotation
		float m_yaw = 0.0f;   // Horizontal rotation

		XMFLOAT3 m_position;
		XMFLOAT3 m_forward_vector;
		XMFLOAT3 m_right_vector;

		XMMATRIX m_view_matrix;
		XMMATRIX m_prev_view_matrix;
		XMMATRIX m_projection_matrix;

		float m_mouse_sensetivity = 1.0f;

		bool m_is_moved = false;
	};

}

module :private;

namespace ysn
{
	Camera::Camera(XMVECTOR position, XMVECTOR focus_point)
	{
		XMStoreFloat3(&m_position, position);

		XMVECTOR direction = XMVectorSubtract(focus_point, position);
		direction = XMVector3Normalize(direction);

		XMStoreFloat3(&m_forward_vector, direction);

		float dir_X = XMVectorGetX(direction);
		float dir_Y = XMVectorGetY(direction);
		float dir_Z = XMVectorGetZ(direction);

		m_yaw = XMConvertToDegrees(std::atan2(dir_X, dir_Z));
		m_pitch = XMConvertToDegrees(std::asin(-dir_Y));

		m_projection_matrix = XMMatrixIdentity();
		m_view_matrix = XMMatrixIdentity();
		m_prev_view_matrix = XMMatrixIdentity();
		m_forward_vector = XMFLOAT3(0, 0, 0);
		m_right_vector = XMFLOAT3(0, 0, 0);

	}

	XMMATRIX Camera::GetViewMatrix() const
	{
		return m_view_matrix;
	}

	XMMATRIX Camera::GetPrevViewMatrix() const
	{
		return m_prev_view_matrix;
	}

	XMMATRIX Camera::GetProjectionMatrix() const
	{
		return m_projection_matrix;
	}

	void Camera::Update()
	{
		m_forward_vector.x = XMScalarCos(XMConvertToRadians(m_pitch)) * XMScalarSin(XMConvertToRadians(m_yaw));
		m_forward_vector.y = -XMScalarSin(XMConvertToRadians(m_pitch));
		m_forward_vector.z = XMScalarCos(XMConvertToRadians(m_pitch)) * XMScalarCos(XMConvertToRadians(m_yaw));

		const XMVECTOR Position = XMVectorSet(m_position.x, m_position.y, m_position.z, 1.0);
		const XMVECTOR FocusPosition =
			XMVectorSet(m_position.x + m_forward_vector.x, m_position.y + m_forward_vector.y, m_position.z + m_forward_vector.z, 0.0);

		const XMVECTOR RightVector = XMVector3Cross(XMLoadFloat3(&Vector3::Up), XMLoadFloat3(&m_forward_vector));

		m_prev_view_matrix = m_view_matrix; // Save it

		m_view_matrix = XMMatrixLookAtRH(Position, FocusPosition, Vector3::Up);
		m_projection_matrix = XMMatrixPerspectiveFovRH(XMConvertToRadians(fov), m_aspect_ratio, m_near_plane, m_far_plane);

		XMStoreFloat3(&m_right_vector, RightVector);

		m_is_moved = false;
	}

	void Camera::SetAspectRatio(const float AspectRatio)
	{
		m_aspect_ratio = AspectRatio;
	}

	XMFLOAT3 Camera::GetPosition() const
	{
		return m_position;
	}

	void Camera::SetPosition(const XMFLOAT3 Position)
	{
		m_position = Position;

		m_is_moved = true;
	}

	void Camera::Move(const XMFLOAT3 Position)
	{
		m_position.x += Position.x;
		m_position.y += Position.y;
		m_position.z += Position.z;

		m_is_moved = true;
	}

	XMFLOAT3 Camera::GetForwardVector() const
	{
		return m_forward_vector;
	}

	XMFLOAT3 Camera::GetRightVector() const
	{
		return m_right_vector;
	}

	XMFLOAT3 Camera::GetUpVector() const
	{
		return Vector3::Up;
	}

	void Camera::SetYaw(const float yaw)
	{
		m_yaw = yaw;

		m_is_moved = true;
	}

	void Camera::SetPitch(const float Pitch)
	{
		m_pitch = Pitch;

		m_is_moved = true;
	}

	float Camera::GetYaw() const
	{
		return m_yaw;
	}

	float Camera::GetPitch() const
	{
		return m_pitch;
	}

	bool Camera::IsMoved()
	{
		return m_is_moved;
	}
}
