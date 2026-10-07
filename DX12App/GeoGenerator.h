#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#include <DirectXMath.h>
#include "Vertex.h"
#include "Skeleton.h"
#include "AnimationClip.h"


class GeoGenerator
{
public:
	
	// 模板网格数据
	template<class T>
	struct TMeshData
	{
		std::vector<T> vertices;
		std::vector<uint32_t> indices;
		DirectX::XMFLOAT3 bound_min;
		DirectX::XMFLOAT3 bound_max;
	};
	//命名模板类型
	using VertexMeshData = TMeshData<Vertex>;
	using ColorMeshData = TMeshData<ColorVertex>;
	using SkeletonMeshData = TMeshData<SkeletonVertex>;
	using PatchData = TMeshData<TerrainVertex>;

	//蒙皮模型数据
	struct SkinnedData
	{
		Skeleton skeleton;                                            // 骨架
		std::vector<std::pair<std::string, SkeletonMeshData>> meshes; // 网格
		std::unordered_map<std::string, AnimationClip> clips;         // 动画片段
		std::vector<std::string> materialNames;                       // 模型材质名
	};

	SkinnedData LoadSkinnedMesh(const std::string fileName);
	VertexMeshData LoadMesh(const std::string fileName);
	VertexMeshData CreatGrid(float width,float depth, uint32_t col, uint32_t row);
	VertexMeshData CreatBox(float width, float height,float depth, uint32_t sub);
	VertexMeshData CreatSphere(float radius, uint32_t row, uint32_t col);
	ColorMeshData  CreateFrustum(float nz, float fz, 
								 float nh, float nw,
								 float fh, float fw);
	PatchData CreateTerrain(float half);

private:
	void Subdivide(VertexMeshData& meshData);//细分
	Vertex MidPoint(const Vertex& v0, const Vertex& v1);//取中心点
};






