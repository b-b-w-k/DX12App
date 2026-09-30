#include "InputManage.h"

InputManage::InputManage()
{
}

InputManage::~InputManage()
{
}

bool InputManage::InputInit()
{

	RegisteredButtom(EAction::MOVE_FRONT, 'W');
	RegisteredButtom(EAction::MOVE_BACK, 'S');
	RegisteredButtom(EAction::MOVE_LEFT, 'A');
	RegisteredButtom(EAction::MOVE_RIGHT,'D');

	RegisteredButtom(EAction::RUNING,   VK_SHIFT);  //shift键
	RegisteredButtom(EAction::JUMP,     VK_SPACE);  //shift键

	RegisteredButtom(EAction::ROT_X_CW, VK_LBUTTON);//左键
	RegisteredButtom(EAction::ROT_X_CCW,VK_LBUTTON);//左键

	RegisteredButtom(EAction::ROT_Y_CW, VK_LBUTTON);//左键
	RegisteredButtom(EAction::ROT_Y_CCW,VK_LBUTTON);//左键

	RegisteredButtom(EAction::ROT_Z_CW, 'Q');
	RegisteredButtom(EAction::ROT_Z_CCW,'E');

	RegisteredButtom(EAction::MOUSE_LEFT, VK_LBUTTON);//左键
	RegisteredButtom(EAction::MOUSE_RIGHT,VK_RBUTTON);//右键

	RegisteredButtom(EAction::CAMERA_FPS, 0x31);//1
	RegisteredButtom(EAction::CAMERA_TPS, 0x32);//2
	RegisteredButtom(EAction::CAMERA_FLY, 0x33);//3

	RegisteredButtom(EAction::MOUSE_LOCK, VK_F1);//F1
	RegisteredButtom(EAction::CONES_LOCK, VK_F2);//F2
	RegisteredButtom(EAction::CONES_VIEW, VK_F3);//F3
	RegisteredButtom(EAction::AABB_VIEW,  VK_F4);//F4
	RegisteredButtom(EAction::OBB_VIEW,   VK_F5);//F5
	RegisteredButtom(EAction::DEBUG_INFO, VK_F6);//F6
	
	return true;
	
}

void InputManage::KeyUp(WPARAM wParam)
{
	m_curKeys[wParam] = false;
	
}

void InputManage::KeyDown(WPARAM wParam)
{
	m_curKeys[wParam] = true;
}

bool InputManage::IsActionStart(EAction action)
{
	auto it = m_actionLib.find(static_cast<uint32_t>(action));
	if (it == m_actionLib.end())return false;
	
	
	return m_prevKeys[it->second] == false&&m_curKeys[it->second]==true;
}

bool InputManage::IsActionEnd(EAction action)
{
	auto it = m_actionLib.find(static_cast<uint32_t>(action));
	if (it == m_actionLib.end())return false;

	return m_prevKeys[it->second] == true && m_curKeys[it->second] == false;
}

bool InputManage::IsActionContinue(EAction action)
{
	auto it = m_actionLib.find(static_cast<uint32_t>(action));
	if (it == m_actionLib.end())return false;

	return m_prevKeys[it->second] == true && m_curKeys[it->second] == true;
}

void InputManage::RegisteredButtom(EAction action, UINT key)
{
	m_actionLib[static_cast<uint32_t>(action)] = key;
}

void InputManage::SetMousePos(POINT mousePos)
{
	m_mousePos = mousePos;
}

POINT InputManage::GetMousePos()
{
	return m_mousePos;
}

POINT InputManage::GetMouseLastPos()
{
	return m_lastPos;
}

POINT InputManage::GetMouseOffset()
{
	return m_offset;
}

void InputManage::EndUpdate()
{
	m_prevKeys = m_curKeys;
	m_lastPos = m_mousePos;
}

void InputManage::UpdateMouse(HWND hWnd, bool isCaptureMouse)
{
	//需要捕获鼠标才进行
	if (!isCaptureMouse)
	{
		m_offset = POINT{ m_mousePos.x - m_lastPos.x, m_mousePos.y - m_lastPos.y };
		return;
	}

	//获取屏幕中心位置
	RECT clientRC;
	GetClientRect(hWnd, &clientRC);
	POINT clientCenter = {
		(clientRC.right - clientRC.left) / 2,
		(clientRC.bottom - clientRC.top) / 2
	};
	POINT screenCenter = clientCenter;
	ClientToScreen(hWnd, &screenCenter);//屏幕中心

	POINT realMouseScreen;
	GetCursorPos(&realMouseScreen);//鼠标位置

	// 计算这一帧偏移
	int dx = realMouseScreen.x - screenCenter.x;
	int dy = realMouseScreen.y - screenCenter.y;

	m_offset = POINT{ dx ,dy };

	// 放回中心
	SetCursorPos(screenCenter.x, screenCenter.y);
}

void InputManage::ResetOffset()
{
	m_offset = { 0,0 };
}

void InputManage::ResetKey()
{
	m_curKeys.fill(false);
	m_prevKeys.fill(false);
	m_offset = { 0, 0 };
}