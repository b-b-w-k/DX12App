#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>

class Skeleton;

// 关键帧,骨骼每一时刻的变换
struct KeyFrame
{
	// 当前时刻
	float curTime;

	// 变换(分开存)
	DirectX::XMFLOAT3 translation;
	DirectX::XMFLOAT3 scale;
	DirectX::XMFLOAT4 rotationQuat; //四元数表示,避免万向节死锁
};

// 单根骨骼的一段动画
struct BoneAnimation
{
	// 获取动画开始时间
	float GetStartTime() const {return keyFrames.empty() ? 0 : keyFrames.front().curTime;}
	// 获取动画结束时间
	float GetEndTime()   const {return keyFrames.empty() ? 0 : keyFrames.back() .curTime;}
	// 插值单根骨骼动画
	void Interpolate(float t, DirectX::XMFLOAT4X4& outMat)const;

	// 单根骨骼的所有关键帧
	std::vector<KeyFrame> keyFrames;
};

// 一段动作(所有骨骼的一段动画)
struct AnimationClip
{
	// 获取所有骨骼动画最先开始时间
	float GetStartTime() const { return boneAnimations.empty() ? 0.0f : boneAnimations.front().GetStartTime(); }
	// 获取所有骨骼动画最后开始时间
	float GetEndTime()   const { return boneAnimations.empty() ? 0.0f : boneAnimations.back() .GetStartTime(); }
	// 仅插值有动画的骨骼
	void Interpolate(float t,std::vector<DirectX::XMFLOAT4X4>& outLocal) const;
	// 插值所有骨骼的动画矩阵(没有动画的用默认矩阵填充)
	void Interpolate(float t, const Skeleton* skeleton,std::vector<DirectX::XMFLOAT4X4>& outLocal) const;

	// 动作名
	std::string name;
	// 所有骨骼动画
	std::vector<BoneAnimation> boneAnimations;
};
