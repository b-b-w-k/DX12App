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
