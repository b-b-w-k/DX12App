#include "ControlComponent.h"
#include "Camera.h"
#include "RayCaster.h"
#include "Node.h"
#include "InputManage.h"
#include "AnimationClip.h"


using namespace DirectX;

void ControlComponent::Update(float dt)
{
	if (state == EState::DIED)return;
	if (!owner || !rayCaster)return;

	//组件更新在变换之前,所以要主动拿第一次的位置
	if (!m_inited)
	{
		auto& I = owner->localTransform;
		m_pos = { I._41, I._42, I._43 };
		m_inited = true;
	}

	UpdateInput();
	UpdateToward(dt);
	UpdateMoveHor(dt);
	UpdateMoveVer(dt);
}

DirectX::XMFLOAT3 ControlComponent::GetScreenForward() const
{
	if (!camera) return { 0, 0, 1 };

	/// 获取相机前向量
	XMFLOAT3 look = camera->GetLook3f();
	// y轴归0
	XMVECTOR dir = XMVectorSetY(XMLoadFloat3(&look), 0.0f);
	//计算长度
	float len = XMVectorGetX(XMVector3Length(dir));
	//归一化输出
	XMFLOAT3 out = { 0, 0, 1 };
	if (len < 1e-5f) return out;

	XMStoreFloat3(&out, dir / len);
	return out;

	
}

DirectX::XMFLOAT3 ControlComponent::GetScreenRight() const
{
	// 获取前向量
	XMFLOAT3 forward = GetScreenForward();
	// 获取上向量
	XMVECTOR up = XMVectorSet(0, 1, 0, 0);
	// 叉积获取右向量
	XMVECTOR right = XMVector3Cross(up, XMLoadFloat3(&forward));
	// 输出
	XMFLOAT3 out = { 1, 0, 0 };
	XMStoreFloat3(&out, XMVector3Normalize(right));
	return out;
}

void ControlComponent::Jump(float dt)
{
}

void ControlComponent::Standby(float dt)
{
}

void ControlComponent::UpdateInput()
{
	// 重置
	m_moveDir = { 0,0,0 };
	m_isRun = false;
	m_isJump = false;

	if (!input)return;

	//获取输入
	//移动
	float dx = 0, dz = 0;
	if (input->IsActionContinue(EAction::MOVE_FRONT))dz += 1;
	if (input->IsActionContinue(EAction::MOVE_BACK))dz -= 1;
	if (input->IsActionContinue(EAction::MOVE_LEFT))dx -= 1;
	if (input->IsActionContinue(EAction::MOVE_RIGHT))dx += 1;

	//奔跑
	m_isRun = input->IsActionContinue(EAction::RUNING);
	//跳跃
	if (input->IsActionEnd(EAction::JUMP) && m_isGround)
		m_isJump = true;

	if (dx == 0.0f && dz == 0.0f) return;

	//摄像机方向
	auto look = GetScreenForward();
	XMVECTOR forward = XMLoadFloat3(&look);
	auto r = GetScreenRight();
	XMVECTOR right = XMLoadFloat3(&r);

	//移动方向
	XMVECTOR dir = forward * dz + right * dx;
	float len = XMVectorGetX(XMVector3Length(dir));
	//归一化
	if (len>1e-5f)
	{
		dir /= len;
		XMStoreFloat3(&m_moveDir, dir);
	}
}

void ControlComponent::UpdateToward(float dt)
{
	//根据移动方向设置人物模型朝向偏移(只绕y轴旋转)
	if (m_moveDir.x != 0.0f || m_moveDir.z != 0.0f)
		m_targetYaw = atan2f(m_moveDir.x, m_moveDir.z);

	float offset = m_targetYaw - m_curYaw;
	//归到(-pi,pi)之间
	while (offset >  XM_PI)offset -= XM_2PI;
	while (offset < -XM_PI)offset += XM_2PI;

	m_curYaw += offset * rotSpeed * dt;

	auto& L = owner->localTransform;
	// 提取缩放
	float sx = XMVectorGetX(XMVector3Length(XMVectorSet(L._11, L._12, L._13, 0)));
	float sy = XMVectorGetX(XMVector3Length(XMVectorSet(L._21, L._22, L._23, 0)));
	float sz = XMVectorGetX(XMVector3Length(XMVectorSet(L._31, L._32, L._33, 0)));
	//获取sin,cos
	float c = cosf(m_curYaw);
	float s = sinf(m_curYaw);
	//写入3x3矩阵(不改变平移)
	L._11 = c * sx;  L._12 = 0.0f; L._13 = -s * sx;
	L._21 = 0.0f;    L._22 = sy;   L._23 = 0.0f;
	L._31 = s * sz;  L._32 = 0.0f; L._33 = c * sz;
	
}

void ControlComponent::UpdateMoveHor(float dt)
{
	// 判断速度
	float speed = m_isRun ? runSpeed : moveSpeed;

	// 计算速度向量
	XMVECTOR v = XMLoadFloat3(&m_moveDir) * speed;
	XMFLOAT3 vel;
	XMStoreFloat3(&vel, v);

	// 获取起始位置
	XMFLOAT3 start = m_pos;
	// 目标位置
	XMFLOAT3 end = { start.x + vel.x * dt,
					 start.y,
					 start.z + vel.z * dt };

	//碰撞检测路径是否被阻挡,获取实际移动距离
	float dist = rayCaster->CheckMove(start, end, owner);

	XMVECTOR offset = XMLoadFloat3(&end) - XMLoadFloat3(&start);
	float offsetLen = XMVectorGetX(XMVector3Length(offset)); //理想移动距离
	// 计算实际人物落点
	XMFLOAT3 resultPos = start;
	if (offsetLen > 1e-5f)
	{
		//最终方向
		XMVECTOR resultDir = offset / offsetLen;
		//最终移动距离
		float resultDist = (std::min)(offsetLen, dist);
		//最终人物落点(x,z)
		XMStoreFloat3(&resultPos, XMLoadFloat3(&start) + resultDist * resultDir);
	}

	//y值靠x,z值算
	m_pos.x = resultPos.x;
	m_pos.z = resultPos.z;
}

void ControlComponent::UpdateMoveVer(float dt)
{
	//获取地面高度
	float groundY = rayCaster->GetGroundHeight(m_pos.x, m_pos.z, owner);
	//没采样到地面高度
	if (groundY <= -FLT_MAX)
	{
		m_isGround = false;
		return;
	}
	//站立高度
	float standY = groundY + feetOffset;
	//写入pos
	m_pos.y = standY;

	m_isGround = true;
	//更新矩阵信息
	auto& L = owner->localTransform;
	L._41 = m_pos.x;
	L._42 = m_pos.y;
	L._43 = m_pos.z;

}

void ControlComponent::ChangeState(EState s)
{
	
}
