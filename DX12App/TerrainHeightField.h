#pragma once
#include<vector>
#include <string>
#include <DirectXMath.h>


// ===== 地形高度场类 =====
// 采样地形高度数据用于碰撞检测
// 高度数据由Scene类传入
class TerrainHeightField
{
public:
	//构建地形高度场
	void LoadHeightData(std::vector<float>&& data,             // CPU高度图数据
						uint32_t width, uint32_t height);      // 高度图宽高          

	void Build(float worldSizeX,float worldSizeY,              // 世界地形宽高(要求宽高一致)
			   float heightScale);                             // 世界地形高度缩放
	
	// 烘培高度图的法线数据,输出高度和法线xz
	std::vector<uint8_t> PackedHeightNormalDate(int bakeStep = 2) const;
	
	// 采样高度(false越界)
	bool SampleHeight(float loaclX, float loaclZ, float& outHeight)const;
	//采样法线(false越界)
	bool SampleNormal(float loaclX, float loaclZ, DirectX::XMFLOAT3& outN)const;
	// 采样坡度 1平地,0垂直(false越界)
	bool SampleSlope(float loaclX, float loaclZ, float& outSlope)const;

	//射线与地形求交
	bool RayCast(const DirectX::XMFLOAT3& origin,
				 const DirectX::XMFLOAT3& direction,
				 float tMax, float& outT) const;

	// ==== 查询成员 ====
	// 是否越界
	bool IsInRange(float loaclX, float loaclZ)const;
	// 获取高度图宽高
	uint32_t GetWidth() const { return m_width; }
	uint32_t GetHeight()const { return m_height; }
	// 获取地形高度缩放
	float GetHeightScale()const { return m_heightScale; }
	// 获取世界地形宽高
	DirectX::XMFLOAT2 GetWorldSize()const { return m_worldSize; }

private:
	// 采样原始高度数据(没做缩放)
	float SampleRaw(float u, float v) const;

	std::vector<float> m_heightData;            // 高度图数据(R8)
	uint32_t m_width = 0;                       // 高度图宽
	uint32_t m_height = 0;                      // 高度图高
	float m_heightScale = 0.0f;                 // 世界地形高度缩放
	DirectX::XMFLOAT2 m_worldSize = { 500,500 };// 世界地形宽高


};			   

