#include "PCH.h"
#include "CollisionComponent.h"


CollisionComponent& CollisionComponent::SetBoundFromCenter(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& extents)
{
	//构建AABB
	bound_AABB.Center  = center;
	bound_AABB.Extents = extents;

	//构建OBB
	DirectX::BoundingOrientedBox::CreateFromBoundingBox(bound_OBB, bound_AABB);

	//构建包围球
	bound_Spheres.Center = center;
	float ex = extents.x;
	float ey = extents.y;
	float ez = extents.z;
	bound_Spheres.Radius = sqrtf(ex * ex + ey * ey + ez * ez);

	//手动设置:标记有效,并且不允许每帧自动刷新覆盖
	isBoundValid  = true;
	isBoundManual = true;

	return *this;
}

CollisionComponent& CollisionComponent::SetBoundFromMinMax(const DirectX::XMFLOAT3& mn, const DirectX::XMFLOAT3& mx)
{
	DirectX::XMFLOAT3 center = 
	{
	   (mn.x + mx.x) * 0.5f,
	   (mn.y + mx.y) * 0.5f,
	   (mn.z + mx.z) * 0.5f
	};
	DirectX::XMFLOAT3 extents =
	{
		(mx.x - mn.x) * 0.5f,
		(mx.y - mn.y) * 0.5f,
		(mx.z - mn.z) * 0.5f
	};
	//调中心点加距离的方法
	return SetBoundFromCenter(center, extents);
}

void CollisionComponent::SetBoundFromWorldAABB(const DirectX::BoundingBox& worldAABB)
{
	//世界空间AABB直接取用
	bound_AABB = worldAABB;

	//构建OBB
	DirectX::BoundingOrientedBox::CreateFromBoundingBox(bound_OBB, bound_AABB);

	//构建包围球
	bound_Spheres.Center = worldAABB.Center;
	float ex = worldAABB.Extents.x;
	float ey = worldAABB.Extents.y;
	float ez = worldAABB.Extents.z;
	bound_Spheres.Radius = sqrtf(ex * ex + ey * ey + ez * ez);

	//自动刷新:只标有效,不动 isBoundManual
	isBoundValid = true;
}
