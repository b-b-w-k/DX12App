#include "RayCaster.h"
#include "Scene.h"
#include "Node.h"
#include "CollisionComponent.h"
#include "RenderComponent.h"

using namespace DirectX;

RayHitInfo RayCaster::RayPick(const Ray& ray) const
{
    //直接发射射线
    return Cast(ray);
}

float RayCaster::CheckMove(const DirectX::XMFLOAT3& start, 
                           const DirectX::XMFLOAT3& end, 
                           Node* self) const
{
   
    //计算移动偏移
    XMVECTOR v = XMLoadFloat3(&end) - XMLoadFloat3(&start);
    float moveLen = XMVectorGetX(XMVector3Length(v));
    if (moveLen < 1e-6f) return 0.0f;//移动太小直接归0

    //通过现在位置,和要移动到的位置构建射线
	Ray ray;
	ray.origin = start;
    XMStoreFloat3(&ray.direction, v / moveLen);

    //发射射线
    RayHitInfo hit = Cast(ray, moveLen, self);//人物移动要剔除自己的碰撞
    //如果击中,说明碰到障碍物了,控制移动距离,太小直接归0
    return hit.isHit ? (std::max)(0.0f, hit.distance - 0.05f) : moveLen;

}

float RayCaster::GetGroundHeight(float x, float z, Node* self) const
{
	Ray ray;
    //给一个很高的位置,垂直向下反射射线
	ray.origin = { x, 100.0f, z }; //y要大于场景最大高度
	ray.direction = { 0, -1, 0 };

    RayHitInfo hit = Cast(ray, 200.0f, self);

    return hit.isHit ? hit.hitPoint.y : -FLT_MAX;//射线击中距离就是人物站立高度
}

float RayCaster::CheckCameraDist(const DirectX::XMFLOAT3& target, 
                                 const DirectX::XMFLOAT3& backDir, 
                                 float maxDist, Node* self) const
{
	Ray ray;
    // 从目标射向摄像机的射线
	ray.origin = target;
	ray.direction = backDir;
    RayHitInfo hit = Cast(ray, maxDist, self);//剔除目标自己
    //返回相机里人物距离
    return hit.isHit ? (std::max)(0.5f, hit.distance - 0.3f) : maxDist;
}

Ray RayCaster::BuildRay(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction)
{

    Ray ray;
	ray.origin = origin; //原点

    //方向
	XMVECTOR dir = XMLoadFloat3(&direction);
	float length = XMVectorGetX(XMVector3Length(dir));
    if (length > 1e-8) 
    {
        dir = XMVector3Normalize(dir);
        XMStoreFloat3(&ray.direction, dir);
    }
    else
    {
        ray.direction = { 0, 0, 1 }; // 默认方向
	}
    return ray;
}

Ray RayCaster::MakeRayFromPoints(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end)
{
	Ray ray;
	ray.origin = start; //原点

    //方向
	XMVECTOR dir = XMLoadFloat3(&end) - XMLoadFloat3(&start);
	float length = XMVectorGetX(XMVector3Length(dir));
    if (length > 1e-8) 
    {
        dir = XMVector3Normalize(dir);
        XMStoreFloat3(&ray.direction, dir);
    }
    else
    {
        ray.direction = { 0, 0, 1 }; // 默认方向
    }
	return ray;

}

Ray RayCaster::MakeRayFromScreen(float screenX, float screenY, 
                                 float screenW, float screenH, 
                                 const DirectX::XMMATRIX& viewProj)
{
    //转到(-1,1)NDC空间
	float ndcX = (screenX / screenW) * 2.0f - 1.0f;
	float ndcY = 1.0f - (screenY / screenH) * 2.0f; // Y 翻转

    //计算逆投影观察矩阵
    XMMATRIX invVP = XMMatrixInverse(nullptr, viewProj);

    //将NDC坐标转为世界坐标下近远平面的两个点
    XMVECTOR pNear = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0, 1), invVP);
    XMVECTOR pFar = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1, 1), invVP);

    //构建射线
    Ray ray;
    XMStoreFloat3(&ray.origin, pNear);//原点
    XMStoreFloat3(&ray.direction, XMVector3Normalize(pFar - pNear));//方向
    return ray;
}

RayHitInfo RayCaster::Cast(const Ray& ray, float maxDist, Node* exclude) const
{
   // 有误,返回初始化击中信息
    RayHitInfo info;
    info.Reset();
    info.distance = maxDist;
    if (!m_scene) return info;

    // 遍历使用碰撞列表里的节点

    for (Node* node : m_scene->GetCollisionNodes())
    {
        if (!node|| node == exclude) continue;
        auto* col = node->GetComponent<CollisionComponent>();
        if (!col || col->collisionType == ECollisionType::None) continue;

        RayHitInfo hit;
        if (IntersectNode(ray, node, 0.0f, info.distance, hit))
        {
            //有击中
            if (hit.distance < info.distance)info = hit;
        }
    }
    return info;
}

bool RayCaster::IntersectNode(const Ray& ray, Node* node, 
                              float tMin, float tMax, 
                              RayHitInfo& outHit) const
{
    
    if (!node)return false;
	auto* col = node->GetComponent<CollisionComponent>();
	if (!col) return false;
    // 不支持碰撞的跳过
    if (col->collisionType == ECollisionType::None)return false;

    //射线信息
	XMVECTOR origin = XMLoadFloat3(&ray.origin);
	XMVECTOR dir = XMLoadFloat3(&ray.direction);


    //先用包围盒粗略筛选
    float tShape;//包围盒命中距离
    if (col->collisionType == ECollisionType::OBB)//支持OBB
    {
        float dist;
        //DX碰撞库里包围盒与射线检测函数,dist为输出距离
        if (!col->bound_OBB.Intersects(origin, dir, dist))return false;
        tShape = dist;
    }
    else // 否则就是支持AABB,三角形也先进行AABB筛选
    {
		float dist;
		if (!col->bound_AABB.Intersects(origin, dir, dist)) return false;
		tShape = dist;
    }
    if (tShape < tMin || tShape > tMax) return false;//有相交但距离不符合也跳过

    // 三角形精细筛选
    if (col->collisionType == ECollisionType::Triangle)
    {
		auto* renderComp = node->GetComponent<RenderComponent>();
		if (!renderComp) return false;

        //世界射线->局部射线,统一在局部坐标系检测
        XMMATRIX invW = XMMatrixInverse(nullptr, XMLoadFloat4x4(&node->worldTransform));
		XMVECTOR localOrigin = XMVector3TransformCoord(origin, invW);
		XMVECTOR localDir = XMVector3TransformNormal(dir, invW);

        //重新归一化
        float dirScale = XMVectorGetX(XMVector3Length(localDir));
        if (dirScale < 1e-8f) return false;//如果缩放太小无法比较,直接返回
        localDir /= dirScale;

        //保存本地射线
        Ray localRay;
		XMStoreFloat3(&localRay.origin, localOrigin);
		XMStoreFloat3(&localRay.direction, localDir);

        //用局部射线与三角形检测

        bool  hit = false;
        float nearest = tShape * dirScale;
        for (auto& sub : renderComp->GetSubmeshs())
        {
            if (!sub.meshGeo)continue;
            float localHit;//局部击中信息(要变换)
			//使用tShape缩小检测范围, tShape * dirScale换算成局部坐标
            if (IntersectMesh(localRay, sub.meshGeo, sub.submeshName,
                0.0f, nearest, localHit))
            {
                nearest = localHit;
                hit = true;
			}
        }
		if (!hit) return false;
		tShape = nearest / dirScale;//变换为世界坐标击中距离
    }

    //填写击中信息
    outHit.Reset();
    outHit.isHit = true;
    outHit.distance = tShape;
    outHit.node = node;
    //计算击中位置(起点+方向*距离)
	outHit.hitPoint.x = ray.origin.x + ray.direction.x * tShape;
	outHit.hitPoint.y = ray.origin.y + ray.direction.y * tShape;
	outHit.hitPoint.z = ray.origin.z + ray.direction.z * tShape;
    return true;
}

bool RayCaster::IntersectMesh(const Ray& ray, 
                              const MeshGeometry* mesh, 
                              const std::string& submeshName, 
                              float tMin, float tMax, 
                              float& outT) const
{
    // 三角形网格与射线求交

    // 如果没有网格返回,CPU端有顶点,索引备份
    if (!mesh || !mesh->indexBufferCPU || !mesh->vertexBufferCPU)return false;
    //如果没有子网格也返回
    auto it = mesh->meshGeometry.find(submeshName);//根据名字查
    if (it == mesh->meshGeometry.end())return false;

    const SubmeshGeometry& sub = it->second;// 否则获取子网格

    //获取顶点/索引数据
    const uint8_t* vb = (const uint8_t*)mesh->vertexBufferCPU->GetBufferPointer();
    const uint8_t* ib = (const uint8_t*)mesh->indexBufferCPU->GetBufferPointer();//固定32

    //顶点取位置
    auto readPos = [&](uint32_t i)->XMVECTOR
        {
            //每次取顶点开头字节
            const uint8_t* v = vb + (size_t)i * mesh->vertexByteStride;
            return XMLoadFloat3((const XMFLOAT3*)v);//往后去XMFLOAT3大小的字节,刚好是pos
        };
    //取索引
    auto raadIndex = [&](uint32_t i)->uint32_t
        {
            //直接当uint32_t类型读
            return ((const uint32_t*)ib)[i];
        };

    //射线信息
    XMVECTOR origin = XMLoadFloat3(&ray.origin);
    XMVECTOR dir = XMLoadFloat3(&ray.direction);

	bool  found = false;  // 是否相交
	float nearest = tMax; // 相交时距离

    //每次取三个顶点围成的三角形遍历
    for (UINT i = 0; i < sub.indexCount; i += 3)
    {
        UINT baseIndex = sub.startIndex + i;

        //子网格的顶点索引要加上偏移才是全局顶点索引
        XMVECTOR v0 = readPos(raadIndex(baseIndex + 0) + sub.vertexOffset);
        XMVECTOR v1 = readPos(raadIndex(baseIndex + 1) + sub.vertexOffset);
        XMVECTOR v2 = readPos(raadIndex(baseIndex + 2) + sub.vertexOffset);

        float dist;
        // DirectXMath库里相交检测函数(射线与三角形),dist为输出的命中距离
        if (TriangleTests::Intersects(origin, dir, v0, v1, v2, dist))
        {
            // 相交取最近距离
            if (dist > tMin && dist < nearest) //tMin,tMax控制相交检测范围
            {
                nearest = dist;
                found = true;
            }
        }
    }

    //输出距离,返回是否成功
    if (found) outT = nearest;
    return found;
}
