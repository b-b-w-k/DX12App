#pragma once
#include <DirectXCollision.h>
#include <d3d12.h>
#include "Component.h"

// 碰撞类型枚举(该对象支持那种碰撞)
enum class ECollisionType
	:uint32_t
{
	None     = 0,
	AABB     = 1,
	OBB      = 2,
	Sphere   = 3,
	Triangle = 4,
};

//碰撞层级掩码
enum class ELayerMask
	:uint32_t
{
	None    = 0,
	Static  = 1 << 0,
	Player  = 1 << 1,
	Monster = 1 << 2,
	ALL     = 0xFFFFFFFF

};

// 碰撞组件
class CollisionComponent
	:public Component
{
public:

	//设置碰撞类型
	CollisionComponent& SetCollisionType(ECollisionType type)
	{
		collisionType = type;
		return *this;
	}

	//设置碰撞箱(中心点加距离)
	CollisionComponent& SetBoundFromCenter(const DirectX::XMFLOAT3& center,
										   const DirectX::XMFLOAT3& extents);

	//设置碰撞箱(min,max)
	CollisionComponent& SetBoundFromMinMax(const DirectX::XMFLOAT3& mn,
										   const DirectX::XMFLOAT3& mx);

	//用世界空间AABB填充(AABB/OBB/包围球),Scene每帧自动刷新时调用
	void SetBoundFromWorldAABB(const DirectX::BoundingBox& worldAABB);

public:

	// 碰撞
	ECollisionType collisionType = ECollisionType::AABB; // 碰撞类型
	DirectX::BoundingBox         bound_AABB    = {};     // 包围盒AABB
	DirectX::BoundingOrientedBox bound_OBB     = {};     // 包围盒OBB
	DirectX::BoundingSphere      bound_Spheres = {};     // 包围球

	// 包围盒有效性:没填充过的盒不参与检测/可视化
	bool isBoundValid  = false;
	// 是否被手动设置过(手动设置的不被每帧自动刷新覆盖)
	bool isBoundManual = false;

	bool isStatic   = true;                              // 是否为静态物体
	bool isTriangle = false;                             // 是否可以进行三角形细筛

	ELayerMask layer = ELayerMask::None;                  // 碰撞层级
	UINT       mask  = UINT_MAX;                          // 碰撞掩码(受那几个层级的碰撞)
};

