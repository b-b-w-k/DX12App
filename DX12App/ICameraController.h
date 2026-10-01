#pragma once

#include <DirectXMath.h>

class InputManage;
class Camera;
class Node;
class RayCaster;


//摄像机控制器接口
struct ICameraController
{
	float rotSpeed;
	float moveSpeed;
	Node* owner = nullptr;
	ICameraController() :rotSpeed(0.15f), moveSpeed(10.0f) {};
	ICameraController(Node* player) :rotSpeed(0.15f), moveSpeed(10.0f) , owner(player){};
	virtual ~ICameraController() = default;
	virtual void UpdateCamControl(float dt, InputManage* pInputManage, Camera* pCamera) = 0;
};

//FPS 第一人称摄像机控制器
struct FPSCameraController
	:public ICameraController
{
	float camHeight = 3; // 摄像机里模型原点的y轴偏移

	//俯仰角限制(0.1f-89.9f)
	float pitchMax = DirectX::XM_PI * 0.25f - 0.1f;//往下
	float pitchMin = 0.1f - DirectX::XM_PI * 0.5f; //往上
	float m_pitch = 0.0f;
	FPSCameraController(Node* player) :ICameraController(player) {};
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
	float camHeight = 2;                     // 摄像机里模型原点的y轴偏移
	RayCaster* rayCaster = nullptr;          // 用于视角钳制的射线检测接口
	DirectX::XMFLOAT3 targetPos = { 0,0,0 }; // 人物所在位置

	// 三轴旋转量
	float theta = 1.5f * DirectX::XM_PI;
	float phi = DirectX::XM_PIDIV4;
	float radius = 5.0f;

	TPSCameraController(Node* player, RayCaster* caster) :ICameraController(player), rayCaster(caster)
	{
		moveSpeed = 0.005f;
	}
	void UpdateCamControl(float dt, InputManage* pInputManage, Camera* pCamera) override;
};
