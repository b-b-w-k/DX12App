#pragma once
#include <DirectXCollision.h> //碰撞库
#include <DirectXMath.h>      
#include <cstring>

class Frustum
{
public:
	//从观察投影矩阵中提取6个平面
	void BulidFrustumFromViewProj(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj)
	{
		DirectX::XMMATRIX P= XMLoadFloat4x4(&proj);
		DirectX::XMMATRIX V= XMLoadFloat4x4(&view);
		//DX碰撞库内置求取视锥体的函数
		DirectX::BoundingFrustum::CreateFromMatrix(frustum, P, false);
		DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, V);
		frustum.Transform(frustum, invView); // 变换到世界空间                                                           
	}

	//返回视锥体
	const DirectX::BoundingFrustum& GetDXFrustum() const { return frustum; }

	bool TestSphere(const DirectX::BoundingSphere& sphere)const
	{
		//用包围球做粗略检测,DISJOINT完全在视锥体外
		return frustum.Contains(sphere) != DirectX::DISJOINT;
		
		
		
	}

	bool TestAABB(const DirectX::BoundingBox& box) const
	{
		return frustum.Contains(box) != DirectX::DISJOINT;
		
	}

	//提取8个角点,用来可视化视锥体
	void GetCorners(DirectX::XMFLOAT3 corners[8]) const
	{
		DirectX::XMFLOAT3 raw[8];
		frustum.GetCorners(raw);
		memcpy(corners, raw, sizeof(DirectX::XMFLOAT3) * 8);
	}

private:
	DirectX::BoundingFrustum frustum;
};

