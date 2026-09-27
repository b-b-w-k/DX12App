#pragma once
#include <DirectXCollision.h>
#include <d3d12.h>
#include "Component.h"


// 碰撞类型枚举(该对象支持那种碰撞)
enum class ECollisionType
{
	None     = 0,
	AABB     = 1,
	OBB      = 2,
	Sphere   = 3,
	Triangle = 4,
};

//碰撞层级掩码
enum class ELayerMask
	:UINT
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

public:

	// 碰撞
	ECollisionType collisionType = ECollisionType::AABB; // 碰撞类型
	DirectX::BoundingBox         bound_AABB;             // 包围盒AABB
	DirectX::BoundingOrientedBox bound_OBB;              // 包围盒OBB
	DirectX::BoundingSphere      bound_Spheres;          // 包围球

	bool isStatic   = true;                              // 是否为静态物体
	bool isTriangle = false;                             // 是否可以进行三角形细筛

	ELayerMask layer = ELayerMask::None;                  // 碰撞层级
	UINT       mask  = UINT_MAX;                          // 碰撞掩码(受那几个层级的碰撞)
};

