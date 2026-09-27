#pragma once

#include <DirectXMath.h>

class InputManage;
class Camera;

using namespace DirectX;

//摄像机控制器接口
struct ICameraController
{
	float rotSpeed;
	float moveSpeed;

	ICameraController() :rotSpeed(0.15f), moveSpeed(10.0f) {};
	virtual ~ICameraController() = default;
	virtual void UpdateCamControl(float dt, InputManage* pInputManage, Camera* pCamera) = 0;
};

//FPS 第一人称摄像机控制器
struct FPSCameraController
	:public ICameraController
{
	//俯仰角限制(0.1f-89.9f)
	float pitchMax = XM_PI * 0.25f - 0.1f;//往下
	float pitchMin = 0.1f - XM_PI * 0.5f; //往上
	float m_pitch = 0.0f;
	FPSCameraController() :ICameraController() {};
	void UpdateCamControl(float dt, InputManage* pInputManage, Camera* pCamera) override;

};

//自由视角摄像机控制器
struct FlyCameraController
	:public ICameraController
{
	FlyCameraController() :ICameraController() {};
	void UpdateCamControl(float dt, InputManage* pInputManage, Camera* pCamera) override;
};

//球面轨道 第三人称摄像机控制器
struct TPSCameraController
	:public ICameraController
{
	XMFLOAT3 targetPos = { 0,0,0 };

	float theta = 1.5f * XM_PI;
	float phi = XM_PIDIV4;
	float radius = 5.0f;

	TPSCameraController() :ICameraController()
	{
		moveSpeed = 0.005f;
	}
	void UpdateCamControl(float dt, InputManage* pInputManage, Camera* pCamera) override;
};
