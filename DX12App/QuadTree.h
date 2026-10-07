#pragma once
#include<vector>
#include "DirectXMath.h"

class Frustum;
class TerrainHeightField;

// 单个保存地形块信息
struct TerrainPatch
{

	DirectX::XMFLOAT2 corners[4];         // 地形网格的四个角点
	DirectX::XMFLOAT3 center = { 0,0,0, };// 地形网格中心
	float subFactor = 1.0f;               // 细分因子
	uint32_t lodLevel = 0;                // lod层级
};

// 四叉树类
// 用于构建地形LOD层级时,提前剔除无用网格

class QuadTree
{
public:

	// 构建四叉树
	void Build(DirectX::XMFLOAT3 offset,       // 世界偏移
			   TerrainHeightField* heightField,// 高度场,采样高度值,做lod距离比较
			   float totalSize,                // 边长总大小(长宽一致)
			   uint32_t patchesPerSide,        // 最大细分时边长分段
			   int maxDepth);                  // 最大细分深度

	// 更新lod,输出需要细分的patch
	void Update(const DirectX::XMFLOAT3& eyePos,
				Frustum* frustum,
				std::vector<TerrainPatch >& outPatches);

	// lod参数
	float subMin = 1.0f;	   // 最远时细分层级
	float subMax = 64.0f;	   // 最近时细分层级
	float maxDist = 80.0f;	   // 最大细分距离(超过这个距离用subMin)

private:

	// 四叉树节点结构体
	struct QuadNode
	{
		// 要保证地形长宽一致,用于遍历网格,构建Patch
		DirectX::XMFLOAT2 worldMin;   // 边界最小值(x,z的)
		DirectX::XMFLOAT2 worldMax;   // 边界最大值(x,z的)
		DirectX::XMFLOAT3 center;     // 地块中心
		int depth;                    // 树深度
		QuadNode* children[4] = {};   // 子节点
		bool isLeaf = true;           // 是否没有子节点
	};

	QuadNode* m_root = nullptr;       // 根节点
	std::vector<QuadNode*> m_allNodes;// 所有节点
	float m_totalSize = 500.0f;       // 边长总大小
	uint32_t m_patchesPerSide = 8;    // 每边最大patch数
	int m_maxDepth = 6;               // 最大深度

	DirectX::XMFLOAT3 m_offset = { 0,0,0 };//相对世界坐标偏移

};

