#include "PCH.h"
#include "Camera.h"


using namespace DirectX;

Camera::Camera()
{
	LookAt(XMFLOAT3(0, 5, -5),XMFLOAT3(0, 0, 0),XMFLOAT3(0, 1, 0));
	SetLens(0.25f * XM_PI, 1.0f, 1.0f, 1000.0f);
}

Camera::~Camera()
{
}

DirectX::XMFLOAT3 Camera::GetPosition3f() const
{
	return m_position;
}

void Camera::SetPosition(float x, float y, float z)
{
	m_position = XMFLOAT3( x,y,z );
	m_isDirty = true;

}

DirectX::XMFLOAT3 Camera::GetRight3f() const
{
	return m_right;
}

DirectX::XMFLOAT3 Camera::GetUp3f() const
{
	return m_up;
}

DirectX::XMFLOAT3 Camera::GetLook3f() const
{
	return m_look;
}

float Camera::GetNearZ() const
{
	return m_nearZ;
}

float Camera::GetFarZ() const
{
	return m_farZ;
}

float Camera::GetAspect() const
{
	return m_aspect;
}

float Camera::GetFovY() const
{
	return m_fovY;
}

float Camera::GetFovX() const
{
	float halfWidth = 0.5f * GetNearWidth();

	return 2.0f * atan(halfWidth / m_nearZ);
}

float Camera::GetNearWidth() const
{
	return m_aspect*m_nearHeight;
}

float Camera::GetNearHeight() const
{
	return m_nearHeight;
}

float Camera::GetFarWidth() const
{
	return m_aspect * m_farHeight;
}

float Camera::GetFarHeight() const
{
	return m_farHeight;
}

void Camera::SetLens(float fovY, float aspect, float nearZ, float farZ)
{
	m_fovY = fovY;
	m_aspect = aspect;
	m_nearZ = nearZ;
	m_farZ = farZ;

	m_nearHeight = 2.0f * m_nearZ * tanf(0.5f * m_fovY);
	m_farHeight = 2.0f * m_farZ * tanf(0.5f * m_fovY);

	XMMATRIX proj = XMMatrixPerspectiveFovLH(m_fovY, m_aspect, m_nearZ, m_farZ);
	XMStoreFloat4x4(&m_proj, proj);
}

void Camera::LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& worldUp)
{
	//转向量
	XMVECTOR posv= XMLoadFloat3(&pos);
	XMVECTOR targetv=XMLoadFloat3(&target);
	XMVECTOR worldUpv= XMLoadFloat3(&worldUp);

	//前向量
	XMVECTOR lookv = XMVector3Normalize(XMVectorSubtract(targetv, posv));
	//右向量
	XMVECTOR rightv = XMVector3Normalize(XMVector3Cross(worldUpv, lookv));
	//上向量
	XMVECTOR CameraUpv = XMVector3Cross(lookv, rightv);

	//保存为float3
	XMStoreFloat3(&m_position, posv);
	XMStoreFloat3(&m_look, lookv);
	XMStoreFloat3(&m_right, rightv);
	XMStoreFloat3(&m_up, CameraUpv);

	m_isDirty = true;

}

DirectX::XMMATRIX Camera::GetView() const
{
	assert(!m_isDirty);
	return XMLoadFloat4x4(&m_view);
}

DirectX::XMMATRIX Camera::GetProj() const
{
	assert(!m_isDirty);
	return XMLoadFloat4x4(&m_proj);
}

DirectX::XMFLOAT4X4 Camera::GetView4x4f() const
{
	assert(!m_isDirty);
	return m_view;
}

DirectX::XMFLOAT4X4 Camera::GetProj4x4f() const
{
	return m_proj;
}

void Camera::MoveFront(float dt)
{
	XMVECTOR dist = XMVectorReplicate(dt);
	XMVECTOR look = XMLoadFloat3(&m_look);
	XMVECTOR pos = XMLoadFloat3(&m_position);
	XMStoreFloat3(&m_position, XMVectorMultiplyAdd(dist, look, pos));

	m_isDirty = true;
}

void Camera::MoveRight(float dt)
{
	XMVECTOR dist = XMVectorReplicate(dt);
	XMVECTOR right = XMLoadFloat3(&m_right);
	XMVECTOR pos = XMLoadFloat3(&m_position);
	XMStoreFloat3(&m_position, XMVectorMultiplyAdd(dist, right, pos));

	m_isDirty = true;
}

void Camera::Pitch(float angle)
{
	XMMATRIX rot = XMMatrixRotationAxis(XMLoadFloat3(&m_right), angle);

	XMStoreFloat3(&m_up, XMVector3TransformNormal(XMLoadFloat3(&m_up), rot));
	XMStoreFloat3(&m_look, XMVector3TransformNormal(XMLoadFloat3(&m_look), rot));
	
	m_isDirty = true;
}

void Camera::Roll(float angle)
{
	XMMATRIX rot = XMMatrixRotationAxis(XMLoadFloat3(&m_look), angle);

	XMStoreFloat3(&m_right, XMVector3TransformNormal(XMLoadFloat3(&m_right), rot));
	XMStoreFloat3(&m_up, XMVector3TransformNormal(XMLoadFloat3(&m_up), rot));

	m_isDirty = true;
}

void Camera::Yaw(float angle)
{
	XMMATRIX rot = XMMatrixRotationY(angle);

	XMStoreFloat3(&m_right, XMVector3TransformNormal(XMLoadFloat3(&m_right), rot));
	XMStoreFloat3(&m_up, XMVector3TransformNormal(XMLoadFloat3(&m_up), rot));
	XMStoreFloat3(&m_look, XMVector3TransformNormal(XMLoadFloat3(&m_look), rot));

	m_isDirty = true;
}

void Camera::UpdateViewMatrix()
{
	if (m_isDirty)
	{
		XMVECTOR rightv = XMLoadFloat3(&m_right);
		XMVECTOR upv = XMLoadFloat3(&m_up);
		XMVECTOR lookv = XMLoadFloat3(&m_look);
		XMVECTOR posv = XMLoadFloat3(&m_position);

		//重新计算,保持正交
		lookv = XMVector3Normalize(lookv);
		upv = XMVector3Normalize(XMVector3Cross(lookv, rightv));
		rightv = XMVector3Cross(upv, lookv);

		float x = -XMVectorGetX(XMVector3Dot(posv, rightv));
		float y = -XMVectorGetX(XMVector3Dot(posv, upv));
		float z = -XMVectorGetX(XMVector3Dot(posv, lookv));

		//保存为float3
		XMStoreFloat3(&m_look, lookv);
		XMStoreFloat3(&m_right, rightv);
		XMStoreFloat3(&m_up, upv);

		//构建观察矩阵
		m_view(0, 0) = m_right.x;
		m_view(1, 0) = m_right.y;
		m_view(2, 0) = m_right.z;
		m_view(3, 0) = x;
		
		m_view(0, 1) = m_up.x;
		m_view(1, 1) = m_up.y;
		m_view(2, 1) = m_up.z;
		m_view(3, 1) = y;

		m_view(0, 2) = m_look.x;
		m_view(1, 2) = m_look.y;
		m_view(2, 2) = m_look.z;
		m_view(3, 2) = z;
		
		m_view(0, 3) = 0.0f;
		m_view(1, 3) = 0.0f;
		m_view(2, 3) = 0.0f;
		m_view(3, 3) = 1.0f;

		m_isDirty = false;
		
	}
}

void Camera::ReOrthogonal()
{
	XMVECTOR look = XMVector3Normalize(XMLoadFloat3(&m_look));
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&m_up), look));
	XMVECTOR up = XMVector3Cross(look, right);

	XMStoreFloat3(&m_look, look);
	XMStoreFloat3(&m_right, right);
	XMStoreFloat3(&m_up, up);
}
