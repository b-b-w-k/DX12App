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

std::vector<uint8_t> TerrainHeightField::PackedHeightNormalDate(int bakeStep) const
{
	if (m_heightData.empty() || m_width < 2 || m_height < 2) return {};

	//间隔和步长
	const float cellX = m_worldSize.x / float(m_width - 1);
	const float cellZ = m_worldSize.y / float(m_height - 1);
	const int   step  = (bakeStep < 1) ? 1 : bakeStep;

	//8字节(rgba16格式)
	std::vector<uint8_t> out((size_t)m_width * m_height * 8);

	//获取原始高度数据
	auto GetH = [&](int x, int y)->float
		{
			x = (x < 0) ? 0 : ((x > (int)m_width - 1) ? (int)m_width - 1 : x);
			y = (y < 0) ? 0 : ((y > (int)m_height - 1) ? (int)m_height - 1 : y);
			return m_heightData[(size_t)y * m_width + x];
		};

	//转为UF16,将(0,1)->(0,65535)
	auto ToU16 = [](float n) -> uint16_t
		{
			n = (std::min)(1.0f, (std::max)(0.0f, n));
			return (uint16_t)lroundf(n * 65535.0f);
		};

	for (uint32_t y = 0; y < m_height; ++y)
	{
		for (uint32_t x = 0; x < m_width; ++x)
		{
			//乘上缩放,因为要算法线
			const float hL = GetH((int)x - step, (int)y) * m_heightScale;
			const float hR = GetH((int)x + step, (int)y) * m_heightScale;
			const float hT = GetH((int)x, (int)y - step) * m_heightScale;
			const float hB = GetH((int)x, (int)y + step) * m_heightScale;

			// 梯度
			const float dhdx = (hR - hL) / (2.0f * step * cellX);
			const float dhdz = (hB - hT) / (2.0f * step * cellZ);

			//法线
			XMVECTOR n = XMVector3Normalize(XMVectorSet(-dhdx, 1.0f, -dhdz, 0.0f));
			XMFLOAT3 nf;
			XMStoreFloat3(&nf, n);

			
			
			//转成uint16_t
			const size_t o = ((size_t)y * m_width + x) * 8;//索引
			const float  h = GetH((int)x, (int)y); //原始高度值

			const uint16_t hr = ToU16(h);                     // 高度
			const uint16_t hg = ToU16(nf.x * 0.5f + 0.5f);    // 法线 x
			const uint16_t hb = ToU16(nf.z * 0.5f + 0.5f);    // 法线 z

			//写入输出

			// 高度
			out[o + 0] = (uint8_t)( hr       & 0xFF);
			out[o + 1] = (uint8_t)((hr >> 8) & 0xFF);
			// 法线 x
			out[o + 2] = (uint8_t)( hg       & 0xFF);
			out[o + 3] = (uint8_t)((hg >> 8) & 0xFF);
			// 法线 z
			out[o + 4] = (uint8_t)( hb       & 0xFF);
			out[o + 5] = (uint8_t)((hb >> 8) & 0xFF);
			// 留空
			out[o + 6] = 0xFF; 
			out[o + 7] = 0xFF;
		}
	}
	return out;
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

bool TerrainHeightField::RayCast(const DirectX::XMFLOAT3& origin, 
								 const DirectX::XMFLOAT3& direction, 
								 float tMax, float& outT) const
{
	if (m_heightData.empty() || m_width < 2 || m_height < 2) return false;
	if (tMax <= 0.0f) return false;

	const float halfX = m_worldSize.x * 0.5f;
	const float halfZ = m_worldSize.y * 0.5f;

	// === 平板法求进入t ===
	// 保证射线在地形采样有效范围内
	//一般情况下(在地形内发出射线)tEnter都是0
	float tEnter = 0.0f;

	const float o[2] = { origin.x,origin.z };
	const float d[2] = { direction.x,direction.z };
	const float h[2] = { halfX,halfZ };
	for (int i=0;i<2;++i)
	{
		if (fabsf(d[i])<1e-8f)
		{
			//如果在范围外直接退出
			if (o[i] < -h[i] || o[i] > h[i]) return false;
			//方向分量太小无法比较跳过
			continue;
		}
		//两个时刻
		//t满足: -h < t*d+o < h
		const float t0 = (-h[i] - o[i]) / d[i];
		const float t1 = ( h[i] - o[i]) / d[i];
		//累记比较进入时刻
		// min判断t0,t1哪个才是进入时刻
		// max判断x ,z平面全部进入才算进入,要取最大的
		tEnter = (std::max)(tEnter, (std::min)(t0, t1));
	}
	if (tEnter > tMax)return false;

	//剔除不合法射线
	//进入点在地表下剔除
	const float sx = direction.x * tEnter + origin.x;
	const float sy = direction.y * tEnter + origin.y;
	const float sz = direction.z * tEnter + origin.z;
	float startH = 0.0f;
	if (!SampleHeight(sx, sz, startH)) return false;//采样越界也直接返回
	if (sy < startH) return false;//如果射线是从地底打上来的剔除


	//=== 垂直射线直接求解(可以直接用xz采样高度场) === 
	
	const float dxz = sqrtf(direction.x * direction.x + direction.z * direction.z);
	if (dxz < 1e-6f)//判断是否垂直与xz平面
	{
		//并且要求方向朝下
		if (direction.y >= 0.0f) return false;
		//sy - startH是垂直落差
		//垂直情况下 -direction.y近似1,这里除是为了统一单位
		//在射线是斜的情况下-direction.y就不是1了
		const float t = tEnter + (sy - startH) / -direction.y;
		if (t < 0.0f || t > tMax) return false;//不合法退出
		outT = t;
		return true;
	}

	//=== 粗步进 求取通用(斜)射线交点 ===
	//每单位纹素的世界距离
	const float cell = (std::max)(m_worldSize.x, m_worldSize.y) / (std::max)(m_width, m_height);
	//每次步进4个纹素的世界距离,钳制最大值为0.25f
	const float dt = (std::max)(cell * 4.0f, 0.25f);
	//控制最大步进次数,避免极端情况
	const int   maxStep = 4096;

	float tStart = tEnter;//左区间:射线开始时刻
	float tEnd   = tEnter;//右区间:射线穿过地形时最近时刻
	bool isPass = false;  //是否穿透地形
	for (int i = 0; i < maxStep; i++)
	{
		tEnd = (std::min)(tStart + dt, tMax);
		const float px = origin.x + direction.x * tEnd;
		const float pz = origin.z + direction.z * tEnd;
		const float py = origin.y + direction.y * tEnd;
		//采样高度
		float h = 0.0f;
		if (!SampleHeight(px, pz, h)) break;
		//如果小于采样高度,证明穿透(前提是默认射线是从上往下打的)
		if (py < h) { isPass = true; break; }
		if (tEnd >= tMax) break; //到设定上限没穿退出
		tStart = tEnd; //准备下一次步进区间
	}
	//未检测到击中,退出
	if (!isPass) return false;

	//通过二分收敛到准确击中位置,16次收敛足够了
	for (int i = 0; i < 16; ++i)
	{
		const float tm = (tStart + tEnd) * 0.5f;
		const float px = origin.x + direction.x * tm;
		const float pz = origin.z + direction.z * tm;
		const float py = origin.y + direction.y * tm;
		//采样高度
		float h = 0.0f;
		if (!SampleHeight(px, pz, h)) { tEnd = tm; continue; }
		//收敛区间
		if (py < h) tEnd   = tm;
		else        tStart = tm;
	}
	outT = (tStart + tEnd) * 0.5f;
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
