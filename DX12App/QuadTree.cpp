
#include "QuadTree.h"
#include "Frustum.h"
#include "TerrainHeightField.h"

using namespace DirectX;

void QuadTree::Build(XMFLOAT3 offset,
					TerrainHeightField* heightField,
					 float totalSize, 
					 uint32_t patchesPerSide, 
					 int maxDepth)
{

	//清理旧数据
	for (auto* n : m_allNodes)delete n;
	m_allNodes.clear();
	// 填充数据
	m_totalSize = totalSize;
	m_patchesPerSide = patchesPerSide;
	m_maxDepth = maxDepth;
	m_offset = offset;

	// ==== 创建根节点 ====
	m_root = new QuadNode();
	m_allNodes.push_back(m_root);

	// 采样地形高度(用局部坐标)
	float h = 0.0f;
	if (heightField)
		heightField->SampleHeight(0, 0, h);
		
	// 填充树节点结构体
	float half = totalSize * 0.5f;
	m_root->worldMin = { offset.x - half, offset.z - half };
	m_root->worldMax = { offset.x + half, offset.z + half };
	m_root->center = { offset.x,  offset.y + h,  offset.z };// 存的是世界坐标的中心
	m_root->depth = 0; //根节点深度为0
	m_root->isLeaf = true;//默认当作是叶子节点(最后一层的节点),后续按情况改变

	//使用显示栈遍历子节点
	std::vector<QuadNode*>stack;
	stack.reserve(1 + 3*maxDepth);//最多入栈节点数

	//首先推入根节点
	stack.push_back(m_root);

	// 最小子节点的边长(用于判断是否可以细分)
	const float baseSize = totalSize / patchesPerSide;

	//开始遍历
	while (!stack.empty())
	{
		//取出末尾节点
		QuadNode* node = stack.back();
		stack.pop_back();

		//本节点边长
		float nodeSize = node->worldMax.x - node->worldMin.x;

		//判断节点是否为最终子节点(最终节点更新时无需展开)
		if (node->depth >= m_maxDepth || baseSize - nodeSize >= 0.0f)
		{
			//最终节点为叶子节点(true)无需展开
			node->isLeaf = true;
			continue;
		}

		// ==== 根据父节点构建4个子节点 ====
		// 每个子节点边长都是父节点的一半
		// 中间四个值
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

			float cY = node->center.y; //默认用父节点高度
			float cX = (cNode->worldMin.x + cNode->worldMax.x) * 0.5f;
			float cZ = (cNode->worldMin.y + cNode->worldMax.y) * 0.5f;
			//采样要用局部坐标
			float localX = cX - offset.x;
			float localZ = cZ - offset.z;
			// 重新采样高度
			if (heightField)
				heightField->SampleHeight(localX, localZ, cY);
			cNode->center ={ cX , offset.y+cY , cZ };// 存的是世界坐标的中心
			cNode->depth = node->depth + 1;
			cNode->isLeaf = true; //默认先设为true,有子节点时才改变

			//存入子节点
			node->children[i] = cNode;
			//标记为不是叶子节点,更新剔除时需要展开
			node->isLeaf = false;//只有存在子节点时才设为false

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

	const float baseSize = m_totalSize / m_patchesPerSide;

	const float projThreshold = 0.02f; //投影阈值,小于这个值就不细分了

	//使用显示栈遍历子节点
	std::vector<QuadNode*>stack;
	stack.reserve(1 + 3 * m_maxDepth);//最多入栈节点数
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
			100.0f, //y方向高度不重要,只要大于地形高度即可
			(node->worldMax.y - node->worldMin.y) * 0.5f
		};

		// 完全在视锥体外剔除,子节点也不需要遍历
		if (!frustum->TestAABB(bound)) continue;

		// 相机到节点中心的距离
		float dx = eyePos.x - node->center.x;
		float dz = eyePos.z - node->center.z;
		float dy = eyePos.y - node->center.y;
		float distSq = dx * dx + dz * dz + dy * dy;
		float dist = sqrtf(distSq);

		//判断是否需要展开
		bool isNeedSubdivide = false;
		if (!node->isLeaf)
		{
			float nodeSize = node->worldMax.x - node->worldMin.x;
			float projSize = nodeSize /( std::max)(dist,1.0f); //投影大小(tan角)

			// 如果投影大于阈值并且子节点的边长大于最小子节点边长,则需要展开
			if(projSize > projThreshold&&(nodeSize*0.5f)>=baseSize)
				isNeedSubdivide = true;
		}

		// 如果需要展开,将子节点压入栈中
		if (isNeedSubdivide)
		{
			for (int i =0;i<4;++i)
				stack.push_back(node->children[i]);
		}
		else
		{
			// 计算细分因子,输出patch信息
		    
			// 细分因子根据距离线性插值
			float t = 1.0f - (std::min)(dist / maxDist, 1.0f);
			float subFactor = subMin + t * (subMax - subMin);

			TerrainPatch p;
			//将世界坐标转回局部坐标
			p.corners[0] = { node->worldMin.x - m_offset.x,node->worldMin.y - m_offset.z };// 左上
			p.corners[1] = { node->worldMax.x - m_offset.x,node->worldMin.y - m_offset.z };// 右上
			p.corners[2] = { node->worldMax.x - m_offset.x,node->worldMax.y - m_offset.z };// 右下
			p.corners[3] = { node->worldMin.x - m_offset.x,node->worldMax.y - m_offset.z };// 左下

			p.center = { node->center.x - m_offset.x,
						 node->center.y - m_offset.y,
						 node->center.z - m_offset.z };
			p.subFactor = subFactor;
			p.lodLevel = node->depth;

			outPatches.push_back(p);
		}

	}

}
