#pragma once
#include "FrameResource.h"

// 构建阴影贴图资源和更新阴影Pass

class ShadowMap
{
public:
	ShadowMap(ID3D12Device* device, UINT size);
	ShadowMap(const ShadowMap&) = delete;
	ShadowMap& operator=(const ShadowMap&) = delete;
	~ShadowMap() = default;
	
	// 建立描述符
	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
						  CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
						  CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

	// 计算灯光视角下View/Proj,填写阴影PassCB
	void UpdateLightMat(const DirectX::XMFLOAT3& lightDirection,
						const DirectX::XMFLOAT3& sceneCenter,
						float sceneRadius,
						const PassConstants& mainPass);
	// 获取成员变量
	ID3D12Resource* GetResource() { return m_resource.Get(); }
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetHCpuDsv() { return m_hCpuDsv; }
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetHGpuSrv() { return m_hGpuSrv; }
	const PassConstants& GetShadowPassCB() const { return m_shadowPassCB; }
	const DirectX::XMFLOAT4X4& GetShadowTransform() const { return m_shadowPassCB.shadowTransform; }

	D3D12_VIEWPORT GetViewport()   const { return m_viewport; }
	D3D12_RECT     GetScreenRect() const { return m_screenRect; }
	UINT           GetSize()       const { return m_size; }
private:
	//建立资源和描述符
	void BuildResources();
	void BuildDescriptors();
	//设备指针
	ID3D12Device* m_d3dDevice = nullptr;
	//纹理尺寸
	UINT m_size = 2048;
	//视口和裁切矩形
	D3D12_VIEWPORT m_viewport = {};
	D3D12_RECT     m_screenRect = {};
	//堆中句柄
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE m_hGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuDsv;

	//GPU资源
	Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
	//阴影Pass数据
	PassConstants m_shadowPassCB;
};

