#include "PCH.h"
#include "TerrainHeightField.h"

using namespace DirectX;

void TerrainHeightField::LoadHeightData(std::vector<float>&& data,
										uint32_t width, uint32_t height)
{
	m_heightData = std::move(data);
	m_width = width;
	m_height = height;
}

void TerrainHeightField::Build(float worldSizeX, float worldSizeY, 
							   float heightScale)
{

	m_worldSize = { worldSizeX, worldSizeY };
	m_heightScale = heightScale;

}

bool TerrainHeightField::SampleHeight(float LocalX, float LocalZ, float& outHeight) const
{
	if(!IsInRange(LocalX, LocalZ)) return false;
	if(m_heightData.empty()) return false;
	
	// 局部坐标->UV坐标
	float u = LocalX / m_worldSize.x + 0.5f;
	float v = LocalZ / m_worldSize.y + 0.5f;
	// 采样原始高度数据
	outHeight = SampleRaw(u, v) * m_heightScale;
	return true;

}

bool TerrainHeightField::SampleNormal(float LocalX, float LocalZ, DirectX::XMFLOAT3& outN) const
{
	// 网格间隔
	float cellX = m_worldSize.x / (m_width - 1);
	float cellZ = m_worldSize.y / (m_height - 1);
	
	//中心点采样高度(用于边界处理)
	float hC;
	if (!SampleHeight(LocalX - cellX, LocalZ, hC)) return false;

	// 周围四个采样点做中心差分(边缘退化成中心点高度,法线朝上)
	float hL, hR, hT, hB;
	if (!SampleHeight(LocalX - cellX, LocalZ, hL))hL = hC;
	if (!SampleHeight(LocalX + cellX, LocalZ, hR))hR = hC;
	if (!SampleHeight(LocalX, LocalZ - cellZ, hT))hT = hC;
	if (!SampleHeight(LocalX, LocalZ + cellZ, hB))hB = hC;

	// 法线
	XMVECTOR n = XMVectorSet((hL - hR) / cellX, 2.0f, (hT - hB) / cellZ, 0.0f);
	n = XMVector3Normalize(n);
	XMStoreFloat3(&outN, XMVector3Normalize(n));
	return true;
}

bool TerrainHeightField::SampleSlope(float LocalX, float LocalZ, float& outSlope) const
{
	XMFLOAT3 n;
	if (!SampleNormal(LocalX, LocalZ, n))return false;
	outSlope = 1.0f - n.y; // 1平地,0垂直
	return true;
}

bool TerrainHeightField::IsInRange(float LocalX, float LocalZ) const
{
	// 据中心的最大范围
	float halfX = m_worldSize.x * 0.5f;
	float halfZ = m_worldSize.y * 0.5f;
	// 据中心的距离
	float dx = LocalX;
	float dz = LocalZ;
	// 判断是否在范围内
	return (dx >= -halfX && dx <= halfX && 
			dz >= -halfZ && dz <= halfZ);
}

float TerrainHeightField::SampleRaw(float u, float v) const
{

	//uv->像素坐标
	float fx = u * (m_width - 1);
	float fz = v * (m_height - 1);

	//取四个方向上的相邻整数坐标进行采样
	UINT x0 = (UINT)fx;
	UINT x1 = std::min(x0 + 1, m_width - 1); //边界钳制
	UINT z0 = (UINT)fz;
	UINT z1 = std::min(z0 + 1, m_height - 1); //边界钳制

	// 插值因子
	float ax = fx - x0;
	float az = fz - z0;

	auto getH = [&](UINT x, UINT z)->float
		{
			return m_heightData[z * m_width + x];
		};
	//4个方向上的高
	float h00 = getH(x0, z0);
	float h10 = getH(x1, z0);
	float h01 = getH(x0, z1);
	float h11 = getH(x1, z1);
	//用x混合
	float h0 = h00 * (1 - ax) + h10 * ax;
	float h1 = h01 * (1 - ax) + h11 * ax;
	//用z混合
	return h0 * (1 - az) + h1 * az;
}
