#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include "DxMath.h"

//定义最大纹理层级
#define MAX_TEXLAYER 4

extern const int numFrameResources;

//材质混合纹理层,定义多层纹理
struct MaterialLayer
{
	int diffMapIndex = -1;
	int norMapIndex = -1;
	float rotation = 0.0f;
	float weight = 1.0f;     //混合权重
	DirectX::XMFLOAT2  uvScale = { 1, 1 };
	DirectX::XMFLOAT2  uvSpeed = { 0, 0 };

};

struct Material
{
	std::string name;

	int matBufferIndex = -1;//材质常量缓冲区索引

	MaterialLayer layers[MAX_TEXLAYER]; //材质纹理层级
	UINT  matLayerCount = 0;

	int numFrameDirty = numFrameResources;//脏标识

	float roughness = 0.25f;
	DirectX::XMFLOAT4 diffAlbedo = { 1.0f,1.0f,1.0f ,1.0f };
	DirectX::XMFLOAT3 fresnelR0 = { 0.01f, 0.01f, 0.01f };
	DirectX::XMFLOAT4X4 matTransform = DxMath::Identity4x4();
};

struct Texture
{
	std::string name;
	std::wstring fileName;
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadHeap = nullptr;
};

struct Light
{
	DirectX::XMFLOAT3 power = { 0.5f,0.5f, 0.5f };//强度
	float decayStart = 1.0f;//衰减开始
	DirectX::XMFLOAT3 direction = { 0.0f,-1.0f,0.0f };//方向
	float decayEnd = 10.0f;//衰减结束
	DirectX::XMFLOAT3 position = { 0.0f,0.0f,0.0f };//位置
	float spotPower = 8.0f;
};
