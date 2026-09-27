#include "AnimationClip.h"
#include "Skeleton.h" 


using namespace DirectX;

//辅助函数,构建变换矩阵SRT
static void BuildLocalMat(const KeyFrame& key, XMFLOAT4X4& outMat)
{
	XMMATRIX S = XMMatrixScaling(key.scale.x, key.scale.y, key.scale.z);
	XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&key.rotationQuat));
	XMMATRIX T = XMMatrixTranslation(key.translation.x, key.translation.y, key.translation.z);
	XMStoreFloat4x4(&outMat, S * R * T);
}

void BoneAnimation::Interpolate(float t, DirectX::XMFLOAT4X4& outMat)const
{
	//空返回
	if (keyFrames.empty())
	{
		XMStoreFloat4x4(&outMat, XMMatrixIdentity());
		return;
	}
	//一个关键帧返回
	if (keyFrames.size() == 1)
	{
		BuildLocalMat(keyFrames.front(), outMat);
		return;
	}

	// 边界检查
	if (t <= GetStartTime())//左
	{
		BuildLocalMat(keyFrames.front(), outMat);
		return;
	}
	if (t >= GetEndTime())//右
	{
		BuildLocalMat(keyFrames.back(), outMat);
		return;
	}


	// ====== 二分查找(找t在哪两个关键帧之间) ======

	for (uint32_t i = 0; i < keyFrames.size() - 1; ++i)
	{
		// 在两帧之间
		if (t >= keyFrames[i].curTime && t <= keyFrames[i + 1].curTime)
		{
			const KeyFrame& k1 = keyFrames[i];
			const KeyFrame& k2 = keyFrames[i+1];

			// ====== 插值(S,T线性插值,R球面插值) ======
			float dt = k2.curTime - k1.curTime;
			float lerp = (dt > 1e-8f) ? (t - k1.curTime) / dt : 0.0f;

			XMVECTOR T1 = XMLoadFloat3(&k1.translation);
			XMVECTOR T2 = XMLoadFloat3(&k2.translation);
			XMVECTOR S1 = XMLoadFloat3(&k1.scale);
			XMVECTOR S2 = XMLoadFloat3(&k2.scale);

			XMVECTOR TLerp = XMVectorLerp(T1, T2, lerp);
			XMVECTOR SLerp = XMVectorLerp(S1, S2, lerp);

			// S,T线性插值
			XMVECTOR R1 = XMLoadFloat4(&k1.rotationQuat);
			XMVECTOR R2 = XMLoadFloat4(&k2.rotationQuat);

			//四元数q,-q表示同一旋转,期望是接近的一边
			//点积小于0,反转一边
			if (XMVectorGetX(XMVector4Dot(R1,R2))<0.0f)
			{
				R1 = XMVectorNegate(R1);
			}
			// R球面插值
			XMVECTOR RSlerp = XMQuaternionSlerp(R1, R2, lerp);

			//插值后的SRT
			XMMATRIX S = XMMatrixScalingFromVector(SLerp);
			XMMATRIX R = XMMatrixRotationQuaternion(RSlerp);
			XMMATRIX T = XMMatrixTranslationFromVector(TLerp);

			XMStoreFloat4x4(&outMat, S * R * T);

			break;
		}
	}

}


void AnimationClip::Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& outLocal) const
{
	outLocal.resize(boneAnimations.size());
	for (int i = 0; i < boneAnimations.size(); ++i)
		boneAnimations[i].Interpolate(t, outLocal[i]);
}

void AnimationClip::Interpolate(float t, const Skeleton* skeleton, std::vector<DirectX::XMFLOAT4X4>& outLocal) const
{
	if (!skeleton)
	{
		outLocal.clear();
		return;
	}

	const int num = skeleton->GetBonesCount();
	outLocal.resize(num);

	for (int i=0;i< num;++i)
	{
		//有动画采样
		if (i < boneAnimations.size() && !boneAnimations[i].keyFrames.empty())
			boneAnimations[i].Interpolate(t, outLocal[i]);
		//没动画用默认绑定姿态矩阵
		else
			outLocal[i] = skeleton->GetBone(i).LocalBindPose;
	}

}
