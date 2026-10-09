#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device,
	uint32_t passCount,
	uint32_t objectCount,
	uint32_t matCount)
{
	//创建命令分配器用于处理帧资源命令
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(cmdAllocator.GetAddressOf())));

	objectCBuffer = std::make_unique<UpLoadBuffer<ObjectConstants>>(device, objectCount, true);
	passCBuffer = std::make_unique<UpLoadBuffer<PassConstants>>(device, passCount, true);
	matBuffer = std::make_unique<UpLoadBuffer<MaterialData>>(device, matCount, false);
	boneBuffer = std::make_unique<UpLoadBuffer<DirectX::XMFLOAT4X4>>(device, maxBones, false);
	terrainBuffer = std::make_unique <UpLoadBuffer<TerrainConstants>>(device, 1, true);
	terrainPatchBuffer = std::make_unique <UpLoadBuffer<TerrainPatchData>>(device, maxTerrainPatches, false);
}

FrameResource::~FrameResource()
{
}
