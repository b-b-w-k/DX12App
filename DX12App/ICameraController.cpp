#include "ICameraController.h"
#include "InputManage.h"
#include "Camera.h"
#include "Node.h"
#include "RayCaster.h"



using namespace DirectX;
void FPSCameraController::UpdateCamControl(float dt, InputManage* pInputManage, Camera* pCamera)
{
	//===========跟随人物(头部)移动===========
	
	
	//y轴移归零
	//XMFLOAT3 look = pCamera->GetLook3f();
	//XMVECTOR lookv = XMLoadFloat3(&look);
	//lookv = XMVector3Normalize(XMVectorSetY(lookv, 0));

	//XMFLOAT3 right = pCamera->GetRight3f();
	//XMVECTOR rightv = XMLoadFloat3(&right);
	//rightv = XMVector3Normalize(XMVectorSetY(rightv, 0));

	//速度
	//float speed = moveSpeed * dt;
	//位置
	//XMFLOAT3 pos = pCamera->GetPosition3f();
	//XMVECTOR posv = XMLoadFloat3(&pos);
	//if (pInputManage->IsActionContinue(EAction::MOVE_FRONT))posv += speed * lookv;  //前移动
	//if (pInputManage->IsActionContinue(EAction::MOVE_BACK)) posv -= speed * lookv;  //后移动
	//if (pInputManage->IsActionContinue(EAction::MOVE_RIGHT))posv += speed * rightv; //右移动
	//if (pInputManage->IsActionContinue(EAction::MOVE_LEFT)) posv -= speed * rightv; //左移动

	//改变摄像机位置
	XMFLOAT3 newPos = { 0,0,0 };
	if (owner)
	newPos = { owner->worldTransform._41, 
			   owner->worldTransform._42+ camHeight,
			   owner->worldTransform._43 };

	pCamera->SetPosition(newPos.x, newPos.y, newPos.z);
	

	//视角移动,无需鼠标按下(视角还是由摄像机控制器控制)

	POINT offset = pInputManage->GetMouseOffset();
	float dx = XMConvertToRadians(rotSpeed * static_cast<float>(offset.x));
	float dy = XMConvertToRadians(rotSpeed * static_cast<float>(offset.y));

	// 偏航
	pCamera->Yaw(dx);

	// 俯仰
	float oldPitch = m_pitch;
	m_pitch = D3DTools::Clamp(m_pitch + dy, pitchMin, pitchMax);//钳制角度
	float newDy = m_pitch - oldPitch;

	if (newDy != 0.0f)
		pCamera->Pitch(newDy);

}

void FlyCameraController::UpdateCamControl(float dt, InputManage* pInputManage, Camera* pCamera)
{
	if (pInputManage->IsActionContinue(EAction::MOVE_FRONT))pCamera->MoveFront(moveSpeed * dt);//前移动
	if (pInputManage->IsActionContinue(EAction::MOVE_BACK)) pCamera->MoveFront(-moveSpeed * dt);//后移动

	if (pInputManage->IsActionContinue(EAction::MOVE_RIGHT))pCamera->MoveRight(moveSpeed * dt);//右移动
	if (pInputManage->IsActionContinue(EAction::MOVE_LEFT))  pCamera->MoveRight(-moveSpeed * dt);//左移动

	if (pInputManage->IsActionContinue(EAction::ROT_Z_CW)) pCamera->Roll(-rotSpeed * dt);//滚转顺
	if (pInputManage->IsActionContinue(EAction::ROT_Z_CCW))pCamera->Roll(rotSpeed * dt);//滚转逆

	if (pInputManage->IsActionContinue(EAction::MOUSE_LEFT))//鼠标左键
	{

		POINT offset = pInputManage->GetMouseOffset();
		float dx = XMConvertToRadians(rotSpeed * static_cast<float>(offset.x));
		float dy = XMConvertToRadians(rotSpeed * static_cast<float>(offset.y));

		pCamera->Yaw(dx);//偏航
		pCamera->Pitch(dy);//俯仰
		
	}
}

void TPSCameraController::UpdateCamControl(float dt, InputManage* pInputManage, Camera* pCamera)
{
	POINT offset = pInputManage->GetMouseOffset();
	float dx = XMConvertToRadians(rotSpeed * static_cast<float>(offset.x));
	float dy = XMConvertToRadians(rotSpeed * static_cast<float>(offset.y));

	if (pInputManage->IsActionContinue(EAction::MOUSE_LEFT))//左键 (绕物体)旋转
	{
		theta += dx;
		phi   += dy;
		phi = D3DTools::Clamp(phi, 0.1f, XM_PI - 0.1f);//钳位极角
	}
	if (pInputManage->IsActionContinue(EAction::MOUSE_RIGHT))//右键 缩放(拉近视角)
	{
		radius += dx - dy;
		radius = D3DTools::Clamp(radius, 3.0f, 15.0f);//3-15
	}

	//获取目标位置(人物位置)
	if (owner)
	{
		targetPos = { owner->worldTransform._41,
					 owner->worldTransform._42 + camHeight,//加上人物高度偏移
					 owner->worldTransform._43 };
	}

	// 先计算方向(目标点->摄像机)
	XMFLOAT3 dir = { sinf(phi) * cosf(theta),
					 cosf(phi),
					 sinf(phi) * sinf(theta)};

	// 进行防穿检测,如果检测到碰撞,则将半径设置为碰撞点距离
	float finalRadius = radius;
	if (owner && rayCaster)
	{
		finalRadius = rayCaster->CheckCameraDist(targetPos, dir, radius, owner);
	}
	//获取目标位置(人物位置)
	XMFLOAT3 newPos = { targetPos.x + dir.x * finalRadius,
						targetPos.y + dir.y * finalRadius,
						targetPos.z + dir.z * finalRadius };
	
	//改变摄像机位置和前向量
	pCamera->LookAt(newPos, targetPos, XMFLOAT3(0, 1, 0));

	
}
