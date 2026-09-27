#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#include <DirectXMath.h>
#include "Vertex.h"

using namespace DirectX;
class GeoGenerator
{
public:
	
	// 模板网格数据
	template<class T>
	struct TMeshData
	{
		std::vector<T> vertices;
		std::vector<uint32_t> indices;
		XMFLOAT3 bound_min;
		XMFLOAT3 bound_max;
	};
	//命名模板类型
	using VertexMeshData = TMeshData<Vertex>;
	using ColorMeshData = TMeshData<ColorVertex>;
	// 定义高度采样器回调函数(根据采样坐标返回高度值)
	using HeightSampler = std::function<float(float u, float v)>;

	
	VertexMeshData LoadMesh(std::string fileName);
	VertexMeshData CreatGrid(float width,float depth, uint32_t col, uint32_t row);
	VertexMeshData CreatBox(float width, float height,float depth, uint32_t sub);
	VertexMeshData CreatSphere(float radius, uint32_t row, uint32_t col);
	ColorMeshData  CreateFrustum(float nz, float fz, 
								 float nh, float nw,
								 float fh, float fw);
	//创建地形(通过高度采样器获取地形高度),支持高度图或程序化生成(二重贝塞尔,正弦波)
	VertexMeshData CreatTerrain(
		float width, float depth,           // 宽高 
		uint32_t col, uint32_t row,         // 行列数
		const HeightSampler& heightSampler, // 高度采样器
		float heightScale = 1.0f,           // 高度缩放
		float uvScale     = 1.0f);          // 纹理缩放

private:
	void Subdivide(VertexMeshData& meshData);//细分
	Vertex MidPoint(const Vertex& v0, const Vertex& v1);//取中心点
};






