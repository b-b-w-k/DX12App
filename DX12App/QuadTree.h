#pragma once
#include<vector>
#include "DirectXMath.h"

class Frustum;
class TerrainHeightField;

// 单个保存地形块信息
struct TerrainPatch
{
	DirectX::XMFLOAT2 corners[4];                 // 地形网格的四个角点
	float edgeTess[4] = { 1.0f,1.0f ,1.0f ,1.0f };// AS最终写入的细分因子
};

// 四叉树类
// 用于构建地形LOD层级时,提前剔除无用网格
// 接口 :
// Bulid  建立四叉树,保存所有树节点
// Update 更新patch,输出所有可见patch

class QuadTree
{
public:

	QuadTree() = default;
	// 析构: 释放所有节点。
	~QuadTree();
	// 成员持有独占裸指针, 禁止拷贝, 避免二次释放
	QuadTree(const QuadTree&) = delete;
	QuadTree& operator=(const QuadTree&) = delete;

	// 构建四叉树
	void Build(DirectX::XMFLOAT3 offset,       // 世界偏移
			   TerrainHeightField* heightField,// 高度场,采样高度值
			   float totalSize,                // 地图尺寸
			   int maxDepth);				   // 四叉树最大深度
			                 

	// 更新lod,输出需要细分的patch
	void Update(const DirectX::XMFLOAT3& eyePos,        // 视线方向
				Frustum* frustum,                       // 视锥体
				std::vector<TerrainPatch >& outPatches);// 输出可见Patch数据

	
	// LOD参数,控制细分层级变化的阈值
	float tessMin  = 1.0f;	   // 最远时细分层级,LOD最小(保证是2的幂)
	float tessMax  = 64.0f;	   // 最近时细分层级,LOD最大(保证是2的幂)
	float maxDist = 80.0f;	   // 最大细分距离(超过这个距离用tessMin),在dist(0,80)之间插值LOD(1,64)

private:

	// 四叉树节点结构体
	// 要保证地形长宽一致
	struct QuadNode
	{
		// 建立时记录
		DirectX::XMFLOAT2 worldMin;          // 边界最小值(x,z的)
		DirectX::XMFLOAT2 worldMax;          // 边界最大值(x,z的)
		DirectX::XMFLOAT3 center;            // 地块中心,用于比较距离
		int depth;                           // 树深度
		QuadNode* children[4] = {};          // 子节点
										     
		// 更新时记录					     
		uint32_t selectFrame = 0;            // 记录是否为前沿节点(用帧id比较只需要一次bool,不需要赋值所有节点)
		float segmentUnit = 2.0f;            // 每单位细分因子(2的幂)
	};

	// 四叉树
	QuadNode* m_root = nullptr;              // 根节点
	std::vector<QuadNode*> m_allNodes;       // 所有节点
	std::vector<QuadNode*> m_frontier;       // 用于输出,前沿节点(可见Patch的节点)

	float m_totalSize = 500.0f;				 // 地图尺寸
	float m_minLeafSize = 62.5f;			 // 最大细分时叶子边长
	int   m_maxDepth = 3;					 // 最大树深度
											 
	uint32_t m_frameID = 1;					 // 当前帧id
											 
	DirectX::XMFLOAT3 m_offset = { 0,0,0 };	 //相对世界坐标偏移


	//获取边上点的位置,根据当前节点和边索引
	static DirectX::XMFLOAT2 EdgePoint(const QuadNode& node, int edgeIndex, float offset);
	//查找前沿节点,按位置
	QuadNode* FindFrontierNode(const DirectX::XMFLOAT2& p) const;
	
};

