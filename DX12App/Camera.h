#pragma once
#include "DxMath.h"

class Camera
{
public:
	Camera();
	~Camera();

	DirectX::XMFLOAT3 GetPosition3f()const;
	void SetPosition(float x, float y, float z);

	DirectX::XMFLOAT3 GetRight3f()const;
	DirectX::XMFLOAT3 GetUp3f()const;
	DirectX::XMFLOAT3 GetLook3f()const;

	float GetNearZ()const;
	float GetFarZ()const;
	float GetAspect()const;
	float GetFovY()const; //垂直视场角
	float GetFovX()const; //水平视场角

	float GetNearWidth()const;
	float GetNearHeight()const;
	float GetFarWidth()const;
	float GetFarHeight()const;

	//水平视场角
	void SetLens(float fovY, float aspect, float nearZ, float farZ);

	void LookAt(
		const DirectX::XMFLOAT3& pos,
		const DirectX::XMFLOAT3& target,
		const DirectX::XMFLOAT3& worldUp);

	//获取矩阵
	DirectX::XMMATRIX GetView()const;
	DirectX::XMMATRIX GetProj()const;

	DirectX::XMFLOAT4X4 GetView4x4f()const;
	DirectX::XMFLOAT4X4 GetProj4x4f()const;

	void MoveFront(float dt); // 前后移动
	void MoveRight(float dt); // 左右移动

	void Pitch(float angle);  // 俯仰角(X)
	void Roll(float angle);   // 翻滚角(Z)
	void Yaw(float angle);    // 偏航角(Y)

	//更新矩阵
	void UpdateViewMatrix();

	//3轴重新正交
	void ReOrthogonal();

private:

	DirectX::XMFLOAT3 m_position = { 0.0f,0.0f ,0.0f };
	DirectX::XMFLOAT3 m_right = { 1.0f,0.0f ,0.0f };
	DirectX::XMFLOAT3 m_up = { 0.0f,1.0f ,0.0f };
	DirectX::XMFLOAT3 m_look = { 0.0f,0.0f ,1.0f };

	float m_nearZ = 0.0f;
	float m_farZ = 0.0f;
	float m_aspect = 0.0f;
	float m_fovY = 0.0f;
	float m_nearHeight = 0.0f;
	float m_farHeight = 0.0f;
	
	bool m_isDirty = true;// 是否脏 标记

	DirectX::XMFLOAT4X4 m_view = DxMath::Identity4x4();
	DirectX::XMFLOAT4X4 m_proj = DxMath::Identity4x4();
};

