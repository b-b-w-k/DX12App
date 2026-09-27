#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXCollision.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "Vertex.h"

//子网格几何体
struct  SubmeshGeometry
{
	UINT indexCount = 0;//索引数量 
	UINT startIndex = 0;//开始索引
	UINT vertexOffset = 0;//顶点偏移

	DirectX::BoundingBox Bounds;     // 包围盒
	DirectX::BoundingSphere Spheres; // 包围球
};

struct MeshGeometry
{
	std::string name;
	//顶点布局标识
	EVertexLayout layout = EVertexLayout::Standard;

	//CPU端数据备份
	Microsoft::WRL::ComPtr<ID3DBlob> vertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> indexBufferCPU = nullptr;
	//GPU端数据
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferGPU = nullptr;
	//上传堆
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferUPload = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferUPload = nullptr;

	UINT vertexByteSize = 0;//顶点缓冲区大小
	UINT vertexByteStride = 0;//步长

	DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT;
	UINT indexByteSize = 0;//索引缓冲区大小


	//子网格参数映射表
	std::unordered_map<std::string, SubmeshGeometry> meshGeometry;

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView()const;

	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView()const;

	void ReleaseUploaders();

};