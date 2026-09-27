#pragma once
#include "D3DTools.h"
#include <array>
#include <unordered_map>


//定义行为枚举
enum class EAction
	:uint32_t
{
	//移动
	MOVE_FRONT,//前
	MOVE_BACK,//后
	MOVE_LEFT,//左
	MOVE_RIGHT,//右
	
	//旋转
	ROT_X_CW,//顺
	ROT_X_CCW,//逆

	ROT_Y_CW,
	ROT_Y_CCW,

	ROT_Z_CW,
	ROT_Z_CCW,

	//鼠标左
	MOUSE_LEFT,
	//鼠标右
	MOUSE_RIGHT,

	CAMERA_FPS,//FPS第一人称摄像机
	CAMERA_TPS,//TPS第三人称摄像机
	CAMERA_FLY,//flyz自由摄像机

	MOUSE_LOCK,//鼠标锁定
	CONES_LOCK,//视锥体锁定
	CONES_VIEW,//视锥体可视化
	AABB_VIEW, //AABB可视化
	OBB_VIEW,  //OBB可视化
	DEBUG_INFO,//调试信息
};

//输入管理类
class InputManage
{
public:
	InputManage();
	~InputManage();
	//初始化
	bool InputInit();
	//按键按下,MsgProc调用
	void KeyUp(WPARAM wParam);
	//按键抬起,MsgProc调用
	void KeyDown(WPARAM wParam);
	//动作是否开始
	bool IsActionStart(EAction action);
	//动作是否结束
	bool IsActionEnd(EAction action);
	//动作是否持续
	bool IsActionContinue(EAction action);
	//注册按键
	void RegisteredButtom(EAction action,UINT key);
	//鼠标位置获取
	void SetMousePos(POINT mousePos);
	//返回鼠标位置
	POINT GetMousePos();
	POINT GetMouseLastPos();
	POINT GetMouseOffset();//获取鼠标偏移
	//帧末更新
	void EndUpdate();
	//更新鼠标位置
	void UpdateMouse(HWND hWnd, bool isCaptureMouse);
	//释放鼠标
	void ResetOffset();
	void ResetKey();
	
private:
	//当前按键状态
	std::array<bool, 256> m_curKeys = {};

	//前一帧按键状态
	std::array<bool, 256> m_prevKeys = {};

	//按键-行为映射表
	std::unordered_map<uint32_t, UINT> m_actionLib;

	//鼠标所在坐标
	POINT m_mousePos = { 0,0 };
	POINT m_lastPos = { 0,0 };
	//鼠标偏移
	POINT m_offset = { 0,0 };

};

