#include "PCH.h"
#include "Scene.h"
#include "Node.h"
#include "DDSTextureLoader.h"
#include "FrameResource.h"
#include "RenderComponent.h"
#include "CollisionComponent.h"
#include "SkeletonComponent.h"
#include "Skeleton.h"
#include "AnimationClip.h"
#include "d3dx12.h"

using namespace DirectX;

Scene::Scene(ID3D12Device* device, 
			 ID3D12GraphicsCommandList* cmdList, 
			 UINT cbvSrvUavDescSize)
	:m_device(device),
	 m_cmdList(cmdList),
	 m_cbvSrvDescSize(cbvSrvUavDescSize)
{
}

Scene::~Scene() = default;

Texture* Scene::RegisterTexture(const std::string& name, 
								const std::wstring& path, 
								bool isCubeMap,
								bool isHeight)
{
	auto tex = std::make_unique<Texture>();
	tex->name = name;
	tex->fileName = path;

	//导入DDS纹理
	DDSTextureLoader loader;
	

	if (!isHeight)
	{
		
		ThrowIfFailed(loader.CreateDDSTextureFromFile12(
			m_device,
			m_cmdList,
			path.c_str(),
			tex->resource,
			tex->uploadHeap));
	}
	else
	{
		//地形高度图
		DDSTextureLoader::DDSTextureResult heightData;
		ThrowIfFailed(loader.CreateDDSTextureFromFile12Ex(
			m_device,
			m_cmdList,
			path.c_str(),
			tex->resource,
			tex->uploadHeap,
			&heightData));

		//地形数据CPU备份
		m_terrainHeightData = heightData.GetNormalizePixels();
		m_terrainHeightW = heightData.pixelWidth;
		m_terrainHeightH = heightData.pixelHeight;
		m_terrainHeightScale = 50.0f; //可调整
		m_isHeightTex = true;


	}

	//将对应名字放入列表,用来创建描述符
	if (isCubeMap) m_skyBoxName = name;    //天空盒单独放
	else m_tex2DList.push_back(name);
	
	//放入纹理表
	m_textures[name] = std::move(tex);
	return m_textures[name].get();
}

Material* Scene::RegisterMaterial(const std::string& name, 
								  const XMFLOAT4& albedo,
								  const XMFLOAT3& fresnelR0, 
								  float roughness, 
								  const std::vector<MaterialLayerDesc>& layerDescs)
{
	auto mat = std::make_unique<Material>();
	mat->name = name;
	mat->matBufferIndex = m_nextMatIndex++;
	mat->diffAlbedo = albedo;
	mat->fresnelR0 = fresnelR0;
	mat->roughness = roughness;

	//查找纹理索引函数
	auto findTexIndex = [&](const std::string texName)->int
		{
			if (texName.empty()) return -1;
			for (UINT i = 0; i < m_tex2DList.size(); ++i)
				if (m_tex2DList[i] == texName)
					return (int)(i + m_srvStartSlot);
			return -1;//-1代表无纹理
		};

	//填写材质层级描述
	mat->matLayerCount = 0;
	for (size_t i = 0; i < layerDescs.size() && i < MAX_TEXLAYER; ++i)
	{
		auto& L = mat->layers[i];
		L.diffMapIndex = findTexIndex(layerDescs[i].diffTex);
		L.norMapIndex = findTexIndex(layerDescs[i].norTex);
		L.uvScale = layerDescs[i].uvScale;
		L.uvSpeed = layerDescs[i].uvSpeed;
		L.rotation = layerDescs[i].rotation;
		L.weight = layerDescs[i].weight;
		mat->matLayerCount++;
	}

	//移入材质表
	m_materials[name] = std::move(mat);
	return m_materials[name].get();
}

Skeleton* Scene::RegisterSkeleton(const std::string& name, Skeleton&& skeleton)
{
	// 构建对象指针
	auto skel = std::make_unique<Skeleton>(std::move(skeleton));
	auto* pSkel = skel.get();
	m_skeletons[name] = std::move(skel);
	return pSkel;
}

AnimationClip* Scene::RegisterAnimate(const std::string& name, AnimationClip&& clip)
{
	// 构建对象指针
	auto anClip = std::make_unique<AnimationClip>(std::move(clip));
	auto* pClip = anClip.get();
	m_animateClips[name] = std::move(anClip);
	return pClip;
}

void Scene::SaveSRVToHeap(ID3D12Device* device,
						 ID3D12DescriptorHeap* srvHeap,
						 UINT startSlot,
						 UINT cbvSrvUavDescSize)
{
	m_device = device;
	m_srvHeap = srvHeap;
	m_srvStartSlot = startSlot;
	m_cbvSrvDescSize = cbvSrvUavDescSize;

	// 统计场景需要的 SRV 数量
	UINT srvCount = (UINT)m_tex2DList.size() +
		(m_skyBoxName.empty() ? 0u : 1u);
	m_srvCount = srvCount;

	// 从startSlot开始
	CD3DX12_CPU_DESCRIPTOR_HANDLE hHeap(
		srvHeap->GetCPUDescriptorHandleForHeapStart(),
		startSlot,
		cbvSrvUavDescSize);

	// SRV描述
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// 2D纹理
	for (auto& name : m_tex2DList)
	{
		auto* tex = m_textures[name].get();
		srvDesc.Format = tex->resource->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex->resource->GetDesc().MipLevels;

		device->CreateShaderResourceView(tex->resource.Get(), &srvDesc, hHeap);
		hHeap.Offset(1, cbvSrvUavDescSize);
	}

	// 天空盒(Cube)
	if (!m_skyBoxName.empty())
	{
		//startSlot+层级纹理偏移
		m_skyBoxSrvIndex = startSlot + (UINT)m_tex2DList.size();

		auto* skyTex = m_textures[m_skyBoxName].get();
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.Format = skyTex->resource->GetDesc().Format;
		srvDesc.TextureCube.MipLevels = skyTex->resource->GetDesc().MipLevels;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

		device->CreateShaderResourceView(skyTex->resource.Get(), &srvDesc, hHeap);
	}
}

Node* Scene::AddNode(const std::string& name, Node* parent)
{

	//构建节点
	auto node = std::make_unique<Node>(name);
	auto* pNode = node.get();

	//放入节点容器和名字表中
	m_nodes.push_back(std::move(node));
	m_nodeByName[name] = pNode;

	//绑定父节点
	if (parent)
	{
		// 双向绑定
		pNode->parent = parent;
		parent->children.push_back(pNode);//同时绑定父的子节点
	}

	//标记更新(重排线性表)
	m_updateDirty = true;
	return pNode;
	
}

Node* Scene::AddAirWall(const std::string& name, 
						const XMFLOAT3& center, 
						const XMFLOAT3& extents, Node* parent)
{
	Node* pNode = AddNode(name, parent);
	//构建组件
	auto* colComp = pNode->AddComponent<CollisionComponent>();
	//将节点加入视图表
	AttachToView(pNode, colComp);
	colComp->SetBoundFromCenter(center, extents);

	return pNode;
}

Node* Scene::AddAirWallMinMax(const std::string& name, 
							  const DirectX::XMFLOAT3& mn, 
							  const DirectX::XMFLOAT3& mx, Node* parent)
{
	Node* pNode = AddNode(name, parent);
	//构建组件
	auto* colComp = pNode->AddComponent<CollisionComponent>();
	//将节点加入视图表
	AttachToView(pNode, colComp);
	colComp->SetBoundFromMinMax(mn, mx);

	return pNode;
}

Node* Scene::AddRenderObject(const std::string& name, 
							 const std::string& geoName, 
							 const std::string& submeshName, 
							 Material* mat, 
							 RenderLayer layer, 
							 Node* parent)
{
	//转发给多子网格
	return AddRenderObject(name, 
						   geoName,
						   { { submeshName, mat, layer } },
						   parent);
}

Node* Scene::AddRenderObject(const std::string& name, 
							 const std::string& geoName, 
							 const std::vector<SubmeshDesc>& submeshDesc, 
							 Node* parent)
{
	Node* pNode = AddNode(name, parent);

	auto* renderComp = pNode->AddComponent<RenderComponent>();
	AttachToView(pNode, renderComp);
	auto* geo = GetGeometry(geoName);
	assert(geo);

	for (auto& desc : submeshDesc)
	{

		//填充渲染组件
		SubmeshDraw& sub = renderComp->AddSubmesh(
			geo,
			desc.submeshName,
			desc.mat,
			desc.layer,
			AllocObjCBIndex());
		sub.primitiveType = desc.primitiveType;
		sub.texTransform = desc.texTransform;

		//载入渲染层级
		m_layers[(int)desc.layer].push_back(&sub);

		//统计信息
		m_totalSubmeshs++;

	}
	return pNode;

}


void Scene::SetParent(Node* child, Node* parent)
{
	if (child) child->SetParent(parent);
}// 设置父节点

void Scene::Detach(Node* child)
{
	if (child) child->Detach();
}

void Scene::RemoveNode(const std::string& name)
{
	//查找对应节点
	auto it = m_nodeByName.find(name);
	if (it == m_nodeByName.end())return;

	Node* pNode = it->second;

	//把要移除的对象的子节点保留(设为独立)
	for(auto* c : pNode->children)
		c->parent = nullptr;
	pNode->children.clear();

	//移除父节点对自身的引用
	if (pNode->parent)
	{
		auto& childs = pNode->parent->children;
		auto itEnd = childs.end();
		childs.erase(std::remove(childs.begin(), itEnd, pNode), itEnd);
		pNode->parent = nullptr;
	}

	//从视图表里移除

	auto RemoveNode = [pNode](std::vector<Node*>& vec)
		{
			vec.erase(std::remove(vec.begin(), vec.end(), pNode), vec.end());
		};

	RemoveNode(m_renderNodes);
	RemoveNode(m_collisionNodes);

	//如果有渲染组件的处理
	if (auto* renderComp=pNode->GetComponent<RenderComponent>())
	{
		//回收所有子网格
		for (auto& sub : renderComp->GetSubmeshs())
		{
			auto& layer = m_layers[(int)sub.renderLayer];
			layer.erase(std::remove(layer.begin(), layer.end(), &sub), layer.end());
			//回收索引
			RecycleObjCBIndex(sub.objCBIndex);
			m_totalSubmeshs--;
		}
	}

	//从节点容器里移除
	auto itEnd = m_nodes.end();
	auto pred = [pNode](const std::unique_ptr<Node>& node)
		{
			return node.get() == pNode; 
		};
	m_nodes.erase(std::remove_if(m_nodes.begin(), itEnd, pred),itEnd);

	//从节点名字表里移除
	m_nodeByName.erase(it);

	//标记更新(重排线性表)
	m_updateDirty = true;
}

Node* Scene::GetNode(const std::string& name)
{
	auto it = m_nodeByName.find(name);
	return it == m_nodeByName.end() ? nullptr : it->second;
}

void Scene::Update(float dt, FrameResource* frame)
{
	// 重置绘制统计
	ResetDrawStats();
	
	// 组件更新
	for (auto& pNode : m_nodes)
	{
		for (auto& comp : pNode->GetComponentAll())
			comp.second->Update(dt);
	}
	

	// 更新所有节点的世界矩阵
	UpdateWorldTransforms();
	// 包围盒更新
	UpdateBounds();
	// 视锥体剔除
	CullFrustum();
	// 更新对象常量缓冲区
	UpdateObjCB(frame);
	// 更新材质缓冲区
	UpdateMatCB(frame);
	//更新骨骼
	UpdateBoneCB(frame);

}

void Scene::Draw(ID3D12PipelineState* pso, 
				 RenderLayer layer, 
				 FrameResource* frame)
{
	//绘制指定渲染层级
	auto& drawLayer = m_layers[(int)layer];
	if (drawLayer.empty()) return;

	// ==== 剔除 ====
	m_visibleList.clear();
	//先将不可见对象剔除
	for (auto* sub : drawLayer)
	{
		auto* renderComp = sub->renderComp;
		if (!renderComp ||
			!renderComp->isVisible ||     // 可见性剔除
			 renderComp->isCull) continue;// 视锥体剔除
		if (!sub->meshGeo)      continue; // 有顶点数据才绘制
		m_visibleList.push_back(sub);
	}
	if (m_visibleList.empty())return;


	// ==== 排序 ====

	//透明对象排序
	if (layer == RenderLayer::Transparent)
	{
		// 按距离排序,透明物体需要从后往前绘制
		auto pred = [this](SubmeshDraw* a, SubmeshDraw* b)
			{
				return DistSqSubmeshToEye(a) > DistSqSubmeshToEye(b);
			};
		std::sort(m_visibleList.begin(), m_visibleList.end(), pred);
	}
	else // 不透明物体排序
	{
		//排序绘制层级里的子网格,方便合批
		auto pred = [](SubmeshDraw* a, SubmeshDraw* b)
			{
				//按几何体(复用顶点)->材质(材质缓存命中率高)->拓扑类型(少一次切换)排序合批
				if (a->meshGeo != b->meshGeo)return a->meshGeo < b->meshGeo;
				if (a->material != b->material)return a->material < b->material;
				return a->primitiveType < b->primitiveType;
			};
		std::sort(m_visibleList.begin(), m_visibleList.end(), pred);
	}

	// ==== 渲染装配 ====

	//设置PSO
	m_cmdList->SetPipelineState(pso);

	//获取常量缓冲区大小,用于地址偏移
	UINT objCBByteSize = D3DTools::BufferByteSize(sizeof(ObjectConstants));
	//获取常量缓冲区资源
	auto objCB = frame->objectCBuffer->Resource();

	//记录上一个网格数据类型
	MeshGeometry* lastGeo = nullptr;
	D3D12_PRIMITIVE_TOPOLOGY lastType = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

	
	//绘制所有当前层级的对象
	for (auto* sub : m_visibleList)
	{
		auto* renderComp = sub->renderComp;
		
		//切换网格几何体时才重新绑定ibv,vbv
		if (sub->meshGeo != lastGeo)
		{
			//设置顶点缓冲区
			auto vbv = sub->meshGeo->GetVertexBufferView();
			m_cmdList->IASetVertexBuffers(0, 1, &vbv);
			//设置索引缓冲区
			auto ibv = sub->meshGeo->GetIndexBufferView();
			m_cmdList->IASetIndexBuffer(&ibv);
			lastGeo = sub->meshGeo;
		}
		//切换拓扑类型才重新绑定
		if (sub->primitiveType != lastType)
		{
			//设置图元拓扑类型
			m_cmdList->IASetPrimitiveTopology(sub->primitiveType);
			lastType = sub->primitiveType;
		}
		
		//先偏移
		D3D12_GPU_VIRTUAL_ADDRESS objCBAdress = objCB->GetGPUVirtualAddress();
		objCBAdress += sub->objCBIndex * objCBByteSize;

		//将常量缓冲取绑定到寄存器b0
		m_cmdList->SetGraphicsRootConstantBufferView(0, objCBAdress);

		//绘制
		m_cmdList->DrawIndexedInstanced(
			sub->submesh.indexCount, 1, sub->submesh.startIndex, sub->submesh.vertexOffset, 0);

		//统计信息
		++m_drawStats.drawnSubmeshs;
		++m_drawStats.drawCalls;
		++m_drawStats.perLayerDrawn[(int)layer];
	}
}

float Scene::GetSamplerBilinear(float u, float v) const
{
	if (m_terrainHeightData.empty())return 0.0f;

	const UINT W = m_terrainHeightW;
	const UINT H = m_terrainHeightH;

	//uv->像素坐标
	float fx = u * (W - 1);
	float fy = v * (H - 1);

	//取四个方向上的相邻整数坐标进行采样
	UINT x0 = (UINT)fx;
	UINT x1 = std::min(x0 + 1, W - 1); //边界钳制
	UINT y0 = (UINT)fy;
	UINT y1 = std::min(y0 + 1, H - 1); //边界钳制

	float ax = fx - x0;
	float ay = fy - y0;
	
	auto getH = [&](UINT x, UINT y)->float
		{
			return m_terrainHeightData[y * W + x];
		};
	//4个方向上的高
	float h00 = getH(x0, y0), h10 = getH(x1, y0);
	float h01 = getH(x0, y1), h11 = getH(x1, y1);
	//用x混合
	float h0 = h00 * (1 - ax) + h10 * ax;
	float h1 = h01 * (1 - ax) + h11 * ax;
	//用y混合
	return h0 * (1 - ay) + h1 * ay;
}

Texture* Scene::GetTexture(const std::string& name)
{
	auto it = m_textures.find(name);
	return it == m_textures.end() ? nullptr : it->second.get();
}

MeshGeometry* Scene::GetGeometry(const std::string& name)
{
	auto it = m_geometries.find(name);
	return it == m_geometries.end() ? nullptr : it->second.get();
}

Material* Scene::GetMaterial(const std::string& name)
{
	auto it = m_materials.find(name);
	return it == m_materials.end() ? nullptr : it->second.get();
}

Skeleton* Scene::GetSkeleton(const std::string& name)
{
	auto it = m_skeletons.find(name);
	return it == m_skeletons.end() ? nullptr : it->second.get();
}

AnimationClip* Scene::GetAnimate(const std::string& name)
{
	auto it = m_animateClips.find(name);
	return it == m_animateClips.end() ? nullptr : it->second.get();
}

void Scene::ReBuildNodeList()
{
	//先清理旧表
	m_nodeList.clear();
	m_nodeList.reserve(m_nodes.size());

	std::queue<Node*> que;

	//按深度划分
	//深度为0开始(推入队列)
	for (auto& node : m_nodes)
		if (node->parent == nullptr)
			que.push(node.get());

	//只需保证深度小的在表前面就行
	while (!que.empty())
	{
		Node* n = que.front();   // 取出首节点
		que.pop();			     // 删除取出的节点
		m_nodeList.push_back(n); // 将同一深度的节点全加入表中

		//推入下一层节点
		for (auto* c : n->children)
			que.push(c);
	}
	//更新完成
	m_updateDirty = false;

}

void Scene::UpdateWorldTransforms()
{
	//要更新的话先更新
	if (m_updateDirty) ReBuildNodeList();

	//用线性表的顺序更新矩阵
	for (Node* n : m_nodeList)
		n->UpdateWorld();
}

void Scene::UpdateBounds()
{
	//更新到世界空间
	for (auto* pNode : m_renderNodes)
	{

		auto* renderComp = pNode->GetComponent<RenderComponent>();
		if (!renderComp) continue;

		//将子网格的AABB合并为一个AABB
		bool first = true;//判断是否为首次合并
		DirectX::BoundingBox merged = {};//合并AABB
		//遍历
		for (auto& sub : renderComp->GetSubmeshs())
		{
			//查找子网格包围盒
			if (!sub.meshGeo)continue;
			auto it = sub.meshGeo->meshGeometry.find(sub.submeshName);
			if (it == sub.meshGeo->meshGeometry.end())continue;

			const auto& localBound = it->second.Bounds;
			XMMATRIX W = XMLoadFloat4x4(&pNode->worldTransform);
			//变换到世界空间AABB
			DirectX::BoundingBox worldBound;
			localBound.Transform(worldBound, W);

			//刚开始merged为空,需要填充
			if (first)
			{
				merged = worldBound;
				first = false;
			}
			else
			{
				//合并包围盒(out,b1,b2)
				DirectX::BoundingBox::CreateMerged(merged, merged, worldBound);
			}
		}
		//将合并完的包围盒交给渲染组件用于剔除
		renderComp->bound_AABB = merged;
		//创建OBB包围盒(根据AABB)
		DirectX::BoundingOrientedBox::CreateFromBoundingBox(renderComp->bound_OBB, merged);

		
	}
}

void Scene::UpdateObjCB(FrameResource* frame)
{
	//常量缓冲区
	auto* objCB = frame->objectCBuffer.get();

	for (auto* pNode : m_renderNodes)
	{
		// 获取渲染组件
		auto* renderComp = pNode->GetComponent<RenderComponent>();
		if (!renderComp)continue;

		bool anyDirty = pNode->IsDirty();//节点脏

		if (!anyDirty)//如果节点不脏,检查子网格是否脏
		{
			for (auto& sub : renderComp->GetSubmeshs())
			{
				//如果该子网格脏,就将该节点设置为要更新
				if (sub.numFrameDirty > 0)
				{
					anyDirty = true;
					break;
				}
			}
		}
		if(anyDirty) // 更新对象常量缓冲区
		{
			XMMATRIX world = XMLoadFloat4x4(&pNode->worldTransform);
			for (auto& sub : renderComp->GetSubmeshs())
			{
				if (sub.numFrameDirty <= 0)continue;
				ObjectConstants objC;
				//更新世界矩阵
				XMStoreFloat4x4(&objC.world, XMMatrixTranspose(world));
				//更新纹理变换矩阵
				XMMATRIX texTransform = XMLoadFloat4x4(&sub.texTransform);
				XMStoreFloat4x4(&objC.texTransform, XMMatrixTranspose(texTransform));
				//更新材质索引
				objC.materialIndex = (sub.material ? sub.material->matBufferIndex : -1);
				//更新骨骼偏移(没有就为0)
				objC.boneOffset = 0;
				if (auto* skel = pNode->GetComponent<SkeletonComponent>())
					objC.boneOffset = skel->m_boneCBOffset;

				objCB->CopyData(sub.objCBIndex, objC);
				sub.numFrameDirty--;
			}
			pNode->numFrameDirty--;
		}
	}
}

void Scene::UpdateMatCB(FrameResource* frame)
{
	//更新材质缓冲区
	auto* matCB = frame->matBuffer.get();
	for (auto& item : m_materials)
	{
		auto mat = item.second.get();

		if (mat->numFrameDirty > 0)
		{
			//更新矩阵和材质数据
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->matTransform);

			MaterialData matData;
			XMStoreFloat4x4(&matData.matTransform, XMMatrixTranspose(matTransform));
			matData.diffAlbedo = mat->diffAlbedo;
			matData.fresnelR0 = mat->fresnelR0;
			matData.roughness = mat->roughness;
			matData.matLayerCount = mat->matLayerCount;
			for (UINT i = 0; i < MAX_TEXLAYER; ++i)
			{
				matData.layers[i].diffMapIndex = mat->layers[i].diffMapIndex;
				matData.layers[i].norMapIndex = mat->layers[i].norMapIndex;
				matData.layers[i].rotation = mat->layers[i].rotation;
				matData.layers[i].uvScale = mat->layers[i].uvScale;
				matData.layers[i].uvSpeed = mat->layers[i].uvSpeed;
				matData.layers[i].weight = mat->layers[i].weight;
			}

			matCB->CopyData(mat->matBufferIndex, matData);
			--mat->numFrameDirty;
		}
	}
}

void Scene::UpdateBoneCB(FrameResource* frame)
{
	auto* boneCB = frame->boneBuffer->Resource();
	static std::vector<XMFLOAT4X4> finalTransform;

	for (auto* pNode : m_skeletonNodes)
	{
		// 获取蒙皮组件
		auto* skelComp = pNode->GetComponent<SkeletonComponent>();
		if (!skelComp || !skelComp->m_skeleton) continue;

		// 视锥体外,不可见的不更新
		auto renderComp = pNode->GetComponent<RenderComponent>();
		if (!renderComp)continue;
		if (renderComp->isCull || !renderComp->isVisible)continue;

		//计算最终变换矩阵
		skelComp->CalFinalTransforms(finalTransform);
		//写入缓冲区
		frame->boneBuffer->CopyData(
			finalTransform.data(),   
			finalTransform.size(),    
			skelComp->m_boneCBOffset); //缓冲区计入开始位置
	}

}

void Scene::AttachToView(Node* node, RenderComponent* comp)
{
	m_renderNodes.push_back(node);
}

void Scene::AttachToView(Node* node, CollisionComponent* comp)
{
	m_collisionNodes.push_back(node);
}

void Scene::AttachToView(Node* node, SkeletonComponent* comp)
{
	m_skeletonNodes.push_back(node);

	// 记录骨骼数量偏移
	// 只有蒙皮组件注册才记录
	uint32_t count = comp->m_skeleton ? comp->m_skeleton->GetBonesCount() : 0;
	// 在Scene中累计骨骼偏移量传给每个蒙皮组件
	comp->m_boneCBOffset = m_nextBoneOffset;
	m_nextBoneOffset += count;
}

int Scene::AllocObjCBIndex()
{
	//优先使用回收槽位
	if (!m_freeCBIndexPool.empty())
	{
		int index = m_freeCBIndexPool.back();
		m_freeCBIndexPool.pop_back();
		return index;
	}
	return m_nextObjCBIndex++;
}

void Scene::RecycleObjCBIndex(int index)
{
	if (index < 0)return;
	//回收索引
	m_freeCBIndexPool.push_back(index);
}

float Scene::DistSqSubmeshToEye(const SubmeshDraw* sub)
{
	if (!sub || !sub->owner)return 0.0f;
	const XMFLOAT4X4 W = sub->owner->worldTransform;
	float dx = W._41 - m_cameraEye.x;
	float dy = W._42 - m_cameraEye.y;
	float dz = W._43 - m_cameraEye.z;
	return dx * dx + dy * dy + dz * dz;
}

void Scene::CullFrustum()
{
	//重置信息
	m_drawStats.totalObjects   = 0;
	m_drawStats.visibleObjects = 0;
	m_drawStats.culledObjects  = 0;


	for (auto* pNode : m_renderNodes)
	{
		//获取渲染组件
		auto* renderComp = pNode->GetComponent<RenderComponent>();
		if (!renderComp) continue;

		

		//天空盒,调试线不会被剔除
		if (renderComp->neverCull)
		{
			renderComp->isCull = false;
		}
		else
		{
			// 用包围盒AABB测试
			renderComp->isCull = !m_frustum.TestAABB(renderComp->bound_AABB);
		}
		
		++m_drawStats.totalObjects;
		//计入信息
		if (!renderComp->isCull)
			 ++m_drawStats.visibleObjects;
		else ++m_drawStats.culledObjects;
	}
}
