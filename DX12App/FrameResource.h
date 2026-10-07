#pragma once
#include <memory>
#include"UpLoadBuffer.h"
#include "RenderData.h"

#define MAX_LIGHTS 16

// 对象常量缓冲区,每个对象更新一次
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 world = DxMath::Identity4x4();//世界矩阵
	DirectX::XMFLOAT4X4 texTransform = DxMath::Identity4x4();//纹理变换矩阵
	uint32_t materialIndex = -1;// 材质索引
	uint32_t boneOffset = 0;    // 骨骼偏移
	uint32_t pad[2] = {0,};

};

// 主渲染常量缓冲区,每帧更新一次
struct PassConstants
{
	DirectX::XMFLOAT4X4 view = DxMath::Identity4x4();//观察矩阵
	DirectX::XMFLOAT4X4 invView = DxMath::Identity4x4();
	DirectX::XMFLOAT4X4 proj = DxMath::Identity4x4();//投影矩阵
	DirectX::XMFLOAT4X4 invProj = DxMath::Identity4x4();
	DirectX::XMFLOAT4X4 viewProj = DxMath::Identity4x4();//观察投影矩阵
	DirectX::XMFLOAT4X4 invViewProj = DxMath::Identity4x4();

	DirectX::XMFLOAT3 eyePosW = { 0,0,0, };//视线方向
	int isClipPlane = 0;//是否启用裁剪平面(0/1)

	DirectX::XMFLOAT2 renderTargetSize = { 0,0 };//渲染目标尺寸(像素->NDC)
	DirectX::XMFLOAT2 invRenderTargetSize = { 0,0 };//倒数(NDC->像素)

	float nearZ = 0.0f;//近平面
	float farZ  = 0.0f;//远平面
	float totalTime = 0.0f;//总时间
	float detalTime = 0.0f;//间隔时间

	DirectX::XMFLOAT4 ambientLight = { 0.0f, 0.0f, 0.0f, 1.0f };//环境光(后期改为贴图)
	Light lights[MAX_LIGHTS];//光源

	DirectX::XMFLOAT4 clipPlane = { 0,1,0,0 };//裁剪平面(反射)

};

// 材质层级数据
struct MaterialLayerData
{
	int       diffMapIndex = -1;
	int       norMapIndex  = -1;
	float     rotation     = 0.0f;
	float     weight       = 1.0f;   
	DirectX::XMFLOAT2  uvScale      = { 1, 1 };
	DirectX::XMFLOAT2  uvSpeed      = { 0, 0 };
	
};

// 材质数据
struct MaterialData
{
	DirectX::XMFLOAT4 diffAlbedo= { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 fresnelR0 = { 0.01f, 0.01f, 0.01f };
	float roughness = 0.25f;

	DirectX::XMFLOAT4X4 matTransform = DxMath::Identity4x4();//纹理变换矩阵

	MaterialLayerData layers[MAX_TEXLAYER];
	int matLayerCount = 0;
	int pad[3] = { 0, };
};

// 整块地形数据
struct TerrainConstants
{
	DirectX::XMFLOAT2 gTerrainSize    = { 500,500 };
	DirectX::XMFLOAT2 gTerrainTexSize = { 256,256 };
	float  gTerrainScale = 50;
	float  pad[3] = { 0,};
};

// 每个地形patch数据
struct TerrainPatchData
{
	DirectX::XMFLOAT2 corner[4] = { {0,0}, }; // 每个patch的四个角点
	float tessFactor = 1.0f;                  // 每个patch的细分因子
	float pad[3] = { 0, };
};

struct FrameResource
{
	FrameResource(ID3D12Device* device,
				  uint32_t passCount, 
				  uint32_t objectCount,
				  uint32_t matCount);
	FrameResource(const FrameResource&) = delete;
	FrameResource& operator=(const FrameResource&) = delete;
	~FrameResource();

	// 支持12个角色*每128根骨骼
	static const uint32_t maxBones = 12 * 128;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator;//命令分配器

	// 资源缓冲区
	std::unique_ptr<UpLoadBuffer<ObjectConstants>>	   objectCBuffer = nullptr;
	std::unique_ptr<UpLoadBuffer<PassConstants>>	   passCBuffer   = nullptr;
	std::unique_ptr<UpLoadBuffer<MaterialData>>		   matBuffer     = nullptr;
	std::unique_ptr<UpLoadBuffer<DirectX::XMFLOAT4X4>> boneBuffer    = nullptr; // 存储至根骨骼变换矩阵
	std::unique_ptr<UpLoadBuffer<TerrainConstants>>    terrainBuffer = nullptr; // 地形常量缓冲区

	uint32_t fence = 0;//围栏值
};




