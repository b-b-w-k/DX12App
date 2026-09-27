#include "SkeletonComponent.h"
#include "AnimationClip.h"
#include "Skeleton.h"

using namespace DirectX;

void SkeletonComponent::Play(AnimationClip* clip, bool isloop)
{
	m_clip = clip;
	m_animTimer = 0.0f;
	m_isPlaying = true;
	m_isLoop = isloop;
}

void SkeletonComponent::Stop()
{
	m_isPlaying = false;
	m_animTimer = 0.0f;
}

void SkeletonComponent::Update(float dt)
{
	if (!m_isPlaying || !m_clip) return;
	//更新时间
	m_animTimer += dt * m_animSpeed;

	float start = m_clip->GetStartTime();
	float end = m_clip->GetEndTime();
	float during = end - start;
	if (during <= 0.0f)return;

	if (m_animTimer > end)
	{
		if (m_isLoop)//循环
		{
			//fmodf:浮点数取余
			m_animTimer = start + fmodf(m_animTimer - start, during);
		}
		else//执行一遍停止
		{
			m_animTimer = end;
			m_isPlaying = false;
		}
	}
}

void SkeletonComponent::CalFinalTransforms(std::vector<DirectX::XMFLOAT4X4>& outMat) const
{
	if (!m_skeleton) 
	{ 
		outMat.clear();
		return;
	}

	//骨骼数量
	const int num = m_skeleton->GetBonesCount();
	outMat.resize(num);
	m_localCache.resize(num);
	m_globalCache.resize(num);

	if (m_clip&& m_skeleton)
	{
		//当前时刻下至父骨骼矩阵(插值过)
		m_clip->Interpolate(m_animTimer, m_skeleton, m_localCache);
	}
	else
	{
		//否则使用默认矩阵
		for (int i = 0; i < num; ++i)
		{
			m_localCache[i] = m_skeleton->GetBone(i).LocalBindPose;
		}
	}
	//累乘至父矩阵,依赖顺序表(父在前)
	for (int i = 0; i < num; ++i)
	{
		XMMATRIX L = XMLoadFloat4x4(&m_localCache[i]);
		int p = m_skeleton->GetBone(i).parentIndex; //根节点是-1
		if (p >= 0)
		{
			// 本骨骼至根节点变换矩阵=本骨骼至父矩阵*父骨骼至根节点变换矩阵 
			XMStoreFloat4x4(&m_globalCache[i], L * XMLoadFloat4x4(&m_globalCache[p]));
		}
		else
			m_globalCache[i] = m_localCache[i];
	}
	// 最后乘 初始绑定姿态矩阵的逆
	for (int i = 0; i < num; ++i)
	{
		XMMATRIX G = XMLoadFloat4x4(&m_globalCache[i]);
		XMMATRIX InvB = XMLoadFloat4x4(&m_skeleton->GetBone(i).InvBindPose);
		XMStoreFloat4x4(&outMat[i], XMMatrixTranspose(InvB * G));
	}

}
