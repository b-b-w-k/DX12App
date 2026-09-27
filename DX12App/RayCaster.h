#pragma once
#include <DirectXMath.h>
#include "MeshGeometry.h"

class Scene;
class SceneObject;
class Node;


//射线结构体
struct Ray
{
	DirectX::XMFLOAT3 origin   = { 0,0,0 };   // 射线起点
	DirectX::XMFLOAT3 direction = { 0,0,1 };  // 射线方向(单位向量)
};

//命中信息
struct RayHitInfo
{
	bool  isHit    = false;                   // 是否命中
	float distance = FLT_MAX;                 // 命中距离
	Node* node     = nullptr;                 // 命中节点
	DirectX::XMFLOAT3 hitPoint = { 0,0,0 };   // 命中点位置

	//重置命中信息
	void Reset()
	{
		isHit     = false;
		distance  = FLT_MAX;
		hitPoint  = { 0,0,0 };
		node      = nullptr;
	}
};

//射线投射类
//用于射线检查
//处理射线与包围盒,三角形的相交检测
class RayCaster
{
public:
	explicit RayCaster(Scene* scene):m_scene(scene){}

	// ===========射线检测接口===========
	// 射线拾取
	RayHitInfo RayPick(const Ray& ray)const;
	// 水平移动碰撞检测
	float CheckMove(const DirectX::XMFLOAT3& start,// 原始位置
					const DirectX::XMFLOAT3& end,  // 要到达位置
					Node* self =nullptr)const;     // 剔除对象
	// 垂直移动碰撞检测
	float GetGroundHeight(float x, float z,            // 要到达的xz坐标
						  Node* self = nullptr) const; // 剔除对象
	// 摄像机防穿阻挡检测
	float CheckCameraDist(const DirectX::XMFLOAT3& target,  // 目标
						  const DirectX::XMFLOAT3& backDir, // 射线方向
						  float maxDist,                    // 最大距离
						  Node* self =nullptr)const;        // 剔除对象
	// ===========射线构造===========
	//构造射线(基本)
	static Ray BuildRay(const DirectX::XMFLOAT3& origin,
						const DirectX::XMFLOAT3& direction);
	// 两点构造射线
	static Ray MakeRayFromPoints(const DirectX::XMFLOAT3& start,
								 const DirectX::XMFLOAT3& end);

	// 屏幕坐标构造射线
	static Ray MakeRayFromScreen(float screenX, float screenY,
								 float screenW, float screenH,
								 const DirectX::XMMATRIX& viewProj);

	// 射线投射,发射射线求交
	RayHitInfo Cast(const Ray& ray,
					float maxDist = FLT_MAX,
					Node* exclude = nullptr) const;
	
private:

	// 检测与对象节点是否碰撞,模型
	bool IntersectNode(const Ray& ray, Node* node,
					   float tMin, float tMax,
					   RayHitInfo& outHit) const;

	// 检测与三角形是否碰撞(局部空间),地形网格
	bool IntersectMesh(const Ray& ray,
					   const MeshGeometry* mesh,
					   const std::string& submeshName,
					   float tMin, float tMax,
					   float& outT) const;

private:
	Scene* m_scene = nullptr;
};

