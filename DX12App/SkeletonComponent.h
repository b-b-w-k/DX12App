#pragma once
#include <vector>
#include <DirectXMath.h>
#include <cstdint>
#include "Component.h"

class Skeleton;
class AnimationClip;

class SkeletonComponent
	:public Component
{
public:
	//播放
	void Play(AnimationClip* clip, bool isloop = true);
	//停止
	void Stop();
	//更新
	void Update(float dt) override;
	//计算最终变换矩阵
	void CalFinalTransforms(std::vector<DirectX::XMFLOAT4X4>& outMat)const;

public:
	Skeleton* m_skeleton  = nullptr; // 骨架树
	AnimationClip* m_clip = nullptr; // 当前动作

	uint32_t  m_boneCBOffset = 0;    // 骨骼缓冲区索引偏移 

	// 动画播放数据
	float m_animTimer = 0.0f;        // 播放计时器
	float m_animSpeed = 1.0f;        // 动画播放速度
	bool  m_isLoop    = true;        // 是否循环播放
	bool  m_isPlaying = false;       // 是否开始播放

private:
	//缓存矩阵数据,避免每帧重算(mutable在const里可改)
	mutable std::vector<DirectX::XMFLOAT4X4>  m_localCache;
	mutable std::vector<DirectX::XMFLOAT4X4>  m_globalCache;

};

