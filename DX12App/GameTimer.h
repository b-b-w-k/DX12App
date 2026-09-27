#pragma once
#include <cstdint>

class GameTimer
{
public:
	GameTimer();

	float TotalTime()const;//获取游戏运行总时间
	float DetalTime()const;//获取帧间隔

	void Reset();//重新设置
	void Start();//继续游戏
	void Pause();//暂停游戏游戏
	void Tick();//增加时间计数
	

private:
	double mSecondsPerCount;//高性能计时器每计数单位对应的秒数
	double m_detalTime;//帧间隔

	int64_t m_basicTime;//游戏开始时刻
	int64_t m_stopTime;//游戏暂停时刻
	int64_t m_curTime;//当前游戏时刻
	int64_t m_pervTime;//先前游戏时刻

	int64_t m_pauseTime;//游戏暂停时间
	

	bool m_isStop;//是否暂停

	
};

