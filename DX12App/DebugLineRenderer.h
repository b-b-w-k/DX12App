#pragma once
#include <d3d12.h>
#include <DirectXCollision.h>
#include <vector>
#include <memory>
#include"UpLoadBuffer.h"
#include "Vertex.h"


//调试线渲染器
//绘制包围盒边界,视锥体边界
class DebugLineRenderer
{
public:

	bool isDebug_AABB     = false;	// AABB可见
	bool isDebug_OBB      = false;	// OBB 可见
	bool isDdebug_Frustum = false;  // 视锥体可见
	bool isLock_Frustum   = false;  // 视锥体锁定

	DirectX::XMFLOAT3 lockedCorners[8] = {};  //缓冲视锥体角点
	bool hasLockedCorners = false;   //是否有角点

public:
	//初始化,创建上传堆
	DebugLineRenderer(ID3D12Device* device, UINT maxVer = 65536);
	//清空顶点vector
	void Begin() { verties.clear(); }
	//绘制
	void Draw(ID3D12GraphicsCommandList* cmdList, ID3D12PipelineState* pso);
	//添加线
	void AddLine(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, const DirectX::XMFLOAT4& color);
	//添加AABB包围盒的8个角点
	void AddAABB(const DirectX::BoundingBox& box, const DirectX::XMFLOAT4& color);
	//添加OBB包围盒的8个角点
	void AddOBB(const DirectX::BoundingOrientedBox& box, const DirectX::XMFLOAT4& color);
	//添加视锥体的8个角点
	void AddFrustum(const DirectX::XMFLOAT3 c[8], const DirectX::XMFLOAT4& color);
private:
	//通过8个角点,构建一个12条边线
	void AddEdge(const DirectX::XMFLOAT3 c[8], const DirectX::XMFLOAT4& color);

	std::unique_ptr<UpLoadBuffer<ColorVertex>> vertexBuffer; //GPU端数据
	std::vector<ColorVertex> verties;// CPU端数据
	UINT max_vertexNum = 0;          // 最大顶点数

	
	
};									   

