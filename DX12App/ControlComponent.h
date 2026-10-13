#pragma once
#include "Component.h"
#include "DirectXMath.h"

class Camera;
class RayCaster;
class Node;
class InputManage;
class AnimationClip;

enum class EState
	:int
{
	MOVE    = 0, 
	JUMP    = 1,
	DIED    = 2,
	STANDBY = 3,
	Count   = 4,
};

// 控制器组件
// 用于角色行为控制
// 接口:
// -移动控制
// -跳跃
// -人称切换
// -模型旋转
class ControlComponent
	:public Component
{
public:
	ControlComponent(Camera* cam, RayCaster* caster, InputManage* in)
		:camera(cam), rayCaster(caster), input(in) {}
	void Update(float dt) override;

public:
	// 控制器参数
	float feetOffset = 0;     // 模型脚底到原点的y轴偏移
	float moveSpeed  = 3.0f;
	float runSpeed   = 6.0f;
	float rotSpeed   = 10.0f; // 镜头旋转速度

	//外部指针
	Camera*      camera    = nullptr; // 摄像机
	RayCaster*   rayCaster = nullptr; // 射线检测
	InputManage* input     = nullptr; // 输入控制


private:
	// 更新
	void UpdateInput();  // 更新输入
	void UpdateToward(float dt); // 更新视角旋转
	void UpdateMoveHor(float dt);// 更新移动水平
	void UpdateMoveVer(float dt);// 更新移动垂直
	// 改变状态
	void ChangeState(EState s);

	// 获取摄像机向量
	DirectX::XMFLOAT3 GetScreenForward()const;// 前向量
	DirectX::XMFLOAT3 GetScreenRight()const;  // 右向量
	void Jump(float dt);
	void Standby(float dt);

private:
	
	EState state = EState::STANDBY;            // 运行时状态

	DirectX::XMFLOAT3 m_moveDir  = { 0,0,0 };  // 移动方向
	DirectX::XMFLOAT3 m_velocity = { 0,0,0 };  // 速度向量
	DirectX::XMFLOAT3 m_pos      = { 0,0,0 };  // 最终位置
	
	// 用于平滑旋转视角
	float m_curYaw = 0;						   // 当前朝向偏移
	float m_targetYaw = 0;					   // 目标朝向偏移
	
	bool m_isGround = false;                   // 是否在地面
	bool m_isJump   = false;				   // 是否跳跃
	bool m_isRun    = false;				   // 是否奔跑
	bool m_inited   = false;                   // 是否初始化
	



};

