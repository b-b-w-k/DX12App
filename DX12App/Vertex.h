#pragma once
#include <DirectXMath.h>

// 顶点布局枚举
enum class EVertexLayout
{
	Standard,
	Color,
	Skeleton,
};

// 标准顶点布局
struct Vertex
{
	Vertex() = default;
	Vertex(
		const DirectX::XMFLOAT3& pos,
		const DirectX::XMFLOAT3& nor,
		const DirectX::XMFLOAT2& UV,
		const DirectX::XMFLOAT3& tan)
		:pos(pos), normal(nor), uv(UV), tangentU(tan) {
	}
	Vertex(
		float px, float py, float pz,
		float nx, float ny, float nz,
		float u, float v,
		float tx, float ty, float tz)
		:pos(DirectX::XMFLOAT3(px, py, pz)),
		normal(DirectX::XMFLOAT3(nx, ny, nz)),
		uv(DirectX::XMFLOAT2(u, v)),
		tangentU(DirectX::XMFLOAT3(tx, ty, tz)) {
	}

	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 uv;
	DirectX::XMFLOAT3 tangentU;

};

// 纯色顶点布局
struct ColorVertex
{
	ColorVertex() = default;
	ColorVertex(
		const DirectX::XMFLOAT3& pos,
		const DirectX::XMFLOAT4& color)
		:pos(pos), color(color) {
	}
	ColorVertex(
		float px, float py, float pz,
		float r, float g, float b, float a)
		:pos(DirectX::XMFLOAT3(px, py, pz)),
		color(DirectX::XMFLOAT4(r, g, b, a)) {
	}

	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;
};

// 骨骼顶点布局
struct SkeletonVertex
{

	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 uv;
	DirectX::XMFLOAT3 tangentU;
	
	float     boneWeights[4]; //支持4块骨骼影响
	uint8_t   boneIndices[4];
};

// 地形顶点布局

struct TerrainVertex
{
	// 只有xz坐标
	DirectX::XMFLOAT2 posXZ;
};