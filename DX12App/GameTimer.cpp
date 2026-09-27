#include "GameTimer.h"
#include <windows.h>


GameTimer::GameTimer()
	:mSecondsPerCount(0.0), m_detalTime(-1.0),
	m_basicTime(0), m_stopTime(0),m_curTime(0),
	m_pervTime(0), m_pauseTime(0),m_isStop(false)
{
	//高性能计时器每计数单位对应的秒数（用于将计数差值转换为秒）
	LARGE_INTEGER  secondsPerCount;
	//获取计数频率
	QueryPerformanceFrequency(&secondsPerCount);
	mSecondsPerCount = 1 / (double)secondsPerCount.QuadPart;
}

float GameTimer::TotalTime()const
{
	if (m_isStop)//暂停时总时间获取
	{
		//(最近暂停的时刻-暂停总时间-游戏开始时刻)*单位换算
		return (float)((m_stopTime - m_pauseTime) - m_basicTime) * mSecondsPerCount;
	}
	else
	{
		//(当前游戏时刻-暂停总时间-游戏开始时刻)*单位换算
		return (float)((m_curTime - m_pauseTime) - m_basicTime) * mSecondsPerCount;
	}

}

float GameTimer::DetalTime()const
{
	return(float)m_detalTime;
}

void GameTimer::Reset()
{
	
	LARGE_INTEGER curTime;
	//获得当前计数值
	QueryPerformanceCounter(&curTime);
	//重新设置游戏开始时刻
	m_basicTime = curTime.QuadPart;
	m_pervTime= curTime.QuadPart;
	//重置暂停时间
	m_pauseTime = 0;
	m_isStop = false;
}

void GameTimer::Start()
{
	LARGE_INTEGER startTime;
	QueryPerformanceCounter(&startTime);
	if (m_isStop)
	{
		//统计暂停时间
		m_pauseTime += startTime.QuadPart - m_stopTime;
		m_pervTime = startTime.QuadPart;
		m_stopTime = 0;
		m_isStop = false;

	}
	

}

void GameTimer::Pause()
{
	if (!m_isStop)
	{
		LARGE_INTEGER curTime;
		//获得当前计数值
		QueryPerformanceCounter(&curTime);
		m_stopTime = curTime.QuadPart;
		m_isStop = true;
	}
}

void GameTimer::Tick()
{
	if (m_isStop)
	{
		//如果游戏停止,帧间隔为0
		m_detalTime = 0.0;
		return;
	}
	LARGE_INTEGER curTime;
	//获得当前计数值
	QueryPerformanceCounter(&curTime);
	//更新当前时刻
	m_curTime = curTime.QuadPart;
	//更新帧间隔
	m_detalTime = (m_curTime - m_pervTime)* mSecondsPerCount;
	//更新先前游戏时刻
	m_pervTime = curTime.QuadPart;

	//防止负数
	if (m_detalTime < 0.0)
	{
		m_detalTime = 0.0;
	}
	
}
