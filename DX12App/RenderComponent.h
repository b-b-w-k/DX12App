#pragma once
#include <deque>
#include "Component.h"
#include "RenderLayer.h"
#include "RenderData.h"
#include "MeshGeometry.h"

class Node;
class RenderComponent;
// 子网格渲染项
// 用于渲染网格所用的信息
struct SubmeshDraw
{
	// 反向引用(用于剔除,变换查询)
	Node*             owner       = nullptr;   // 节点对象指针,查询变换信息
	RenderComponent*  renderComp  = nullptr;   // 渲染组件指针,查询是否剔除

	// 顶点数据		          
	MeshGeometry*    meshGeo      = nullptr;                   // 网格几何体
	std::string      submeshName;                              // 子网格名字
	SubmeshGeometry  submesh      = {};                        // 子网格数据
	EVertexLayout    layout       = EVertexLayout::Standard;   // 顶点布局
						     			   
	// 着色器数据	         			   					   
	Material*           material      = nullptr;               // 材质数据
	int                 objCBIndex    = -1;                    // 常量缓冲区索引
	DirectX::XMFLOAT4X4 texTransform  = DxMath::Identity4x4(); // 纹理变换

	// 实例化绘制字段
	uint32_t instStride = 0;                                   // 实例字节数
	uint32_t instCount = 0;                                    // 实例化数
	D3D12_GPU_VIRTUAL_ADDRESS instAddress = 0;                 // 实例缓冲区地址
	

	// 渲染状态
	RenderLayer              renderLayer   = RenderLayer::Opaque;                    // 渲染层级
	D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;  // 图元拓扑类型

	// 脏标识
	int numFrameDirty = numFrameResources;
	void MarkDirty() { numFrameDirty = numFrameResources; }
};


// 渲染组件
// 渲染对象
class RenderComponent
	: public Component

{
public:
	// 添加子网格渲染数据
	SubmeshDraw& AddSubmesh(MeshGeometry* geo,             // 网格几何体
							const std::string& submeshName,// 子网格名字
							Material* mat,                 // 子网格材质
							RenderLayer layer,             // 子网格渲染层级
							UINT objCBIndex);              // 子网格常量缓冲区索引
	// 查找子网格
	SubmeshDraw* GetSubmesh(const std::string& submeshName);
	// 获取所有子网格
	std::deque<SubmeshDraw>& GetSubmeshs() { return m_submeshes; }

	// 更新标记(让所有子网格数据更新)
	void MarkDirty();

	//一键构建方法(所有子网格材质,纹理变换相同)
	void SetMaterialAll(Material* mat);
	void SetTexTransformAll(float u, float v);

	
public:
	bool neverCull = false;                  // 永远不会剔除(天空盒,调试线)
	bool isCull    = true;                   // 是否剔除
	bool isVisible = true;                   // 是否可见(暂时隐藏)

private:
	std::deque<SubmeshDraw> m_submeshes;     // 子网格渲染项(双端队列,扩容指针不失效)
};

