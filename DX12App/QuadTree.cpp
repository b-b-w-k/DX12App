#include "PCH.h"
#include "QuadTree.h"
#include "Frustum.h"
#include "TerrainHeightField.h"

using namespace DirectX;

QuadTree::~QuadTree()
{
	
	for (auto* n : m_allNodes)
		delete n;
	m_allNodes.clear();
	m_root = nullptr;

}

void QuadTree::Build(XMFLOAT3 offset,
					 TerrainHeightField* heightField,
					 float totalSize, 
					 int maxDepth)
{

	//清理旧数据
	for (auto* n : m_allNodes)delete n;
	m_allNodes.clear();
	// 填充数据
	m_totalSize = totalSize;
	m_offset = offset;
	m_maxDepth = (std::max)(0, (std::min)(8, maxDepth));//0,8之间
	//计算最小叶子边长
	m_minLeafSize = m_totalSize / (1u << m_maxDepth);

	// ==== 创建根节点 ====	
	m_root = new QuadNode();
	m_allNodes.push_back(m_root);

	// 采样地形高度(用局部坐标采样)
	float h = 0.0f;
	if (heightField)
		heightField->SampleHeight(0, 0, h);
		
	// 填充树节点结构体
	float half = totalSize * 0.5f;
	m_root->worldMin = { offset.x - half, offset.z - half };//中心局部坐标是(0,0,0)
	m_root->worldMax = { offset.x + half, offset.z + half };
	m_root->center = { offset.x,  offset.y + h,  offset.z };// 存的是世界坐标的中心
	m_root->depth = 0; //根节点深度为0

	//使用显示栈遍历子节点
	std::vector<QuadNode*>stack;
	stack.reserve(4*(m_maxDepth+1));//最多入栈节点数

	//首先推入根节点
	stack.push_back(m_root);

	//开始遍历
	while (!stack.empty())
	{
		//取出末尾节点
		QuadNode* node = stack.back();
		stack.pop_back();

		//判断节点是否为最终子节点
		if (node->depth >= m_maxDepth) continue;

		// ==== 根据父节点构建4个子节点 ====
		// 每个子节点边长都是父节点的一半
		// 中间值
		float mX = (node->worldMin.x + node->worldMax.x) * 0.5f;
		float mZ = (node->worldMin.y + node->worldMax.y) * 0.5f;

		//子节点最小值
		const float childMin[4][2] =
		{
			{node->worldMin.x, node->worldMin.y},// 左上
			{mX, node->worldMin.y},              // 右上
			{node->worldMin.x, mZ},              // 左下
			{mX, mZ}                             // 右下 
		};
		//子节点最大值
		const float childMax[4][2] =
		{
			{mX,mZ},                             // 左上
			{node->worldMax.x,mZ},				 // 右上
			{mX,node->worldMax.y},				 // 左下
			{node->worldMax.x,node->worldMax.y}	 // 右下
		};

		//构建子节点
		for (int i = 0; i < 4; ++i)
		{
			QuadNode* cNode = new QuadNode();
			m_allNodes.push_back(cNode);

			cNode->worldMin = { childMin[i][0], childMin[i][1] };
			cNode->worldMax = { childMax[i][0], childMax[i][1] };

			float cY = node->center.y - offset.y; //失败回退为父节点的世界高度
			float cX = (cNode->worldMin.x + cNode->worldMax.x) * 0.5f;
			float cZ = (cNode->worldMin.y + cNode->worldMax.y) * 0.5f;
			//采样要用局部坐标
			float localX = cX - offset.x;
			float localZ = cZ - offset.z;
			// 采样子节点高度
			if (heightField)
				heightField->SampleHeight(localX, localZ, cY);
			cNode->center ={ cX , cY + offset.y , cZ };// 存的是世界坐标的中心
			cNode->depth = node->depth + 1;//深度加一
			//存入子节点
			node->children[i] = cNode;
			//将子节点压入栈中
			stack.push_back(cNode);
		}

	}

}

void QuadTree::Update(const DirectX::XMFLOAT3& eyePos, 
					  Frustum* frustum, 
					  std::vector<TerrainPatch >& outPatches)
{
	//清理输出
	outPatches.clear();
	if (!m_root)return;

	//帧id自增
	++m_frameID;//用于判别前沿节点
	m_frontier.clear();//清理前沿节点

	// 投影阈值,小于这个值就不细分了
	// 通过patch在平面上所占的比例来决定LOD,而不是只按距离
	// (2.0f*m_minLeafSize)为maxdepth-1时节点的边长
	// projThreshold与占据屏幕的面积呈正比
	const float projThreshold = (2.0f*m_minLeafSize)/(std::max)(maxDist,1.0f); 

	//使用显示栈遍历子节点
	std::vector<QuadNode*>stack;
	stack.reserve(4 * (m_maxDepth + 1));//最多入栈节点数
	stack.push_back(m_root);

	while (!stack.empty())
	{
		//取出末尾节点
		QuadNode* node = stack.back();
		stack.pop_back();

		// ==== 视锥体剔除 ====
		// 计算节点包围盒
		BoundingBox bound;
		bound.Center = node->center;
		bound.Extents =
		{
			(node->worldMax.x - node->worldMin.x) * 0.5f,
			100.0f, //y方向高度不重要,只要大于地形高度,能包住节点即可
			(node->worldMax.y - node->worldMin.y) * 0.5f
		};

		// 完全在视锥体外剔除,子节点也不需要遍历
		if (!frustum->TestAABB(bound)) continue;

		// 余下节点按投影阈值,判断是否需要展开

		// 相机到节点中心的距离
		float dx = eyePos.x - node->center.x;
		float dy = eyePos.y - node->center.y;
		float dz = eyePos.z - node->center.z;
		float distSq = dx * dx + dz * dz + dy * dy;
		float dist = sqrtf(distSq);

		// 当前的投影屏幕占比(tan角)
		float nodeSize = node->worldMax.x - node->worldMin.x;
		float projSize = nodeSize / (std::max)(dist, 1.0f); //投影大小(tan角)

		//判断是否需要展开
		bool isNeedTess = false;
		if (node->depth < m_maxDepth)
		{
			// 如果投影大于阈值,则需要展开
			if(projSize > projThreshold)
				isNeedTess = true;
		}

		// 如果需要展开,将子节点压入栈中,继续进行细分判断
		if (isNeedTess)
		{
			for (int i =0;i<4;++i)
				stack.push_back(node->children[i]);
		}
		else //否则当前节点为前沿节点直接输出
		{
			// === 计算细分因子,输出patch信息 ===

			// 每单位张角对应细分数量(标准细分单位)
			const float tessUnit = tessMax / projThreshold;
			// 细分因子 =标准细分单位*当前的投影屏幕占比(tan角)
			float tessFactor = tessUnit * projSize;
			// 防止越界
			tessFactor = (std::max)(tessMin, (std::min)(tessFactor, tessMax));

			// 算出该节点分段数
			// 每单位叶子边长段数 =总段数/单位数(有几个最小叶子边长)
			const float segUnit = tessFactor / (nodeSize / m_minLeafSize);
			//向上取整(偶数)
			float segmentUnit = 2 * ceil(segUnit / 2);
			node->segmentUnit = segmentUnit;//记录分段数

			node->selectFrame = m_frameID;//记录为本帧前沿节点
			m_frontier.push_back(node);//存储本前沿节点

			TerrainPatch p;
			// 角点:将世界坐标转回局部坐标
			p.corners[0] = { node->worldMin.x - m_offset.x,node->worldMin.y - m_offset.z };// 左上
			p.corners[1] = { node->worldMax.x - m_offset.x,node->worldMin.y - m_offset.z };// 右上
			p.corners[2] = { node->worldMax.x - m_offset.x,node->worldMax.y - m_offset.z };// 右下
			p.corners[3] = { node->worldMin.x - m_offset.x,node->worldMax.y - m_offset.z };// 左下

			outPatches.push_back(p);
		}

	}

	//对齐邻边细分因子
	const float offset = m_minLeafSize * 0.01f;//转到邻居节点的偏移量
	for (size_t i = 0; i < m_frontier.size(); ++i)
	{
		QuadNode* node = m_frontier[i];
		const float nodeSize = node->worldMax.x - node->worldMin.x;
		//边长能被分的段数
		const float nSegment = (float)(1u << (m_maxDepth - node->depth));
		for (int e = 0; e < 4; ++e)//四边分别比较
		{
			float gMin = node->segmentUnit;
			//获取邻居节点
			QuadNode* neib = FindFrontierNode(EdgePoint(*node, e, offset));
			if (neib)
			{
				const float neibSize = neib->worldMax.x - neib->worldMin.x;

				gMin = (std::min)(gMin, neib->segmentUnit);
				//只让粗邻居影响自己
				if (neibSize >= nodeSize - 1e-3f)
					gMin = (std::min)(gMin, neib->segmentUnit);

			}
			outPatches[i].edgeTess[e] = nSegment * gMin;
		}
	}

}

QuadTree::QuadNode* QuadTree::FindFrontierNode(const DirectX::XMFLOAT2& p) const
{
	QuadNode* n = m_root;
	while (n && n->selectFrame != m_frameID)//是本帧前沿节点就继续
	{
		if (n->depth>m_maxDepth) return nullptr;
		const float mX = (n->worldMin.x + n->worldMax.x) * 0.5f;
		const float mZ = (n->worldMin.y + n->worldMax.y) * 0.5f;
		const int ci = (p.x >= mX ? 1 : 0) + (p.y >= mZ ? 2 : 0);//子节点索引
		n = n->children[ci];//继续下沉
	}
	return n;
}

DirectX::XMFLOAT2 QuadTree::EdgePoint(const QuadNode& node, int edgeIndex, float offset)
{
	const float mx = (node.worldMin.x + node.worldMax.x) * 0.5f;
	const float mz = (node.worldMin.y + node.worldMax.y) * 0.5f;
	switch (edgeIndex)
	{
	case 0: return { mx, node.worldMin.y - offset };	// minZ 边外侧
	case 1: return { node.worldMax.x + offset, mz };    // maxX 边外侧
	case 2: return { mx, node.worldMax.y + offset };	// maxZ 边外侧
	case 3: return { node.worldMin.x - offset, mz };    // minX 边外侧
	}
	return { mx, mz };
	
}
