#include "PCH.h"
#include "DebugLineRenderer.h"
#include "Vertex.h"


using namespace DirectX;
DebugLineRenderer::DebugLineRenderer(ID3D12Device* device, UINT maxVer)
{
	vertexBuffer = std::make_unique<UpLoadBuffer<ColorVertex>>(device, maxVer, false);
	max_vertexNum = maxVer;
}

void DebugLineRenderer::Draw(ID3D12GraphicsCommandList* cmdList, ID3D12PipelineState* pso)
{
	if (verties.empty()) return;
	if (verties.size() > max_vertexNum)verties.resize(max_vertexNum);//丢弃范围外的顶点
	
	vertexBuffer->CopyData(verties.data(), (UINT)verties.size(),0);

	D3D12_VERTEX_BUFFER_VIEW vbv = {};
	vbv.BufferLocation = vertexBuffer->Resource()->GetGPUVirtualAddress();
	vbv.StrideInBytes = sizeof(ColorVertex);
	vbv.SizeInBytes = (UINT)sizeof(ColorVertex) * verties.size();

	cmdList->SetPipelineState(pso);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	cmdList->IASetVertexBuffers(0, 1, &vbv);
	cmdList->DrawInstanced((UINT)verties.size(), 1, 0, 0);
}

void DebugLineRenderer::AddLine(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT4& color)
{
	verties.emplace_back(a, color);
	verties.emplace_back(b, color);
}

void DebugLineRenderer::AddAABB(const BoundingBox& box, const XMFLOAT4& color)
{
	XMFLOAT3 c[8];
	box.GetCorners(c);
	AddEdge(c, color);
}

void DebugLineRenderer::AddOBB(const BoundingOrientedBox& box, const XMFLOAT4& color)
{
	XMFLOAT3 c[8];
	box.GetCorners(c);
	AddEdge(c, color);
}

void DebugLineRenderer::AddFrustum(const XMFLOAT3 c[8], const XMFLOAT4& color)
{
	AddEdge(c, color);
}

void DebugLineRenderer::AddEdge(const XMFLOAT3 c[8], const XMFLOAT4& color)
{
	// 近平面
	AddLine(c[0], c[1], color); AddLine(c[1], c[2], color);
	AddLine(c[2], c[3], color); AddLine(c[3], c[0], color);
	// 远平面
	AddLine(c[4], c[5], color); AddLine(c[5], c[6], color);
	AddLine(c[6], c[7], color); AddLine(c[7], c[4], color);
	// 连接近远平面
	AddLine(c[0], c[4], color); AddLine(c[1], c[5], color);
	AddLine(c[2], c[6], color); AddLine(c[3], c[7], color);
}
