#include "ShadowMap.h"

using namespace DirectX;

ShadowMap::ShadowMap(ID3D12Device* device, UINT size)
	:m_d3dDevice(device), m_size(size)
{
	m_viewport = { 0.0f, 0.0f, (float)size, (float)size, 0.0f, 1.0f };
	m_screenRect = { 0, 0, (LONG)size, (LONG)size };

	BuildResources();
}

void ShadowMap::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, 
								 CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, 
								 CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
	m_hCpuSrv = hCpuSrv;
	m_hGpuSrv = hGpuSrv;
	m_hCpuDsv = hCpuDsv;
	BuildDescriptors();
}

void ShadowMap::UpdateLightMat(const DirectX::XMFLOAT3& lightDirection, 
									const DirectX::XMFLOAT3& sceneCenter, 
									float sceneRadius,
									const PassConstants& mainPass)
{

	//通过光源位置和场景包围球半径计算view/proj

	XMVECTOR L = XMVector3Normalize(XMLoadFloat3(&lightDirection)); // 光线传播方向
	XMVECTOR center = XMLoadFloat3(&sceneCenter);

	
	// 深度范围留出余量(完全盖住整个场景)
	const float depth = sceneRadius * 2.0f + 100.0f;
	XMVECTOR eye = XMVectorSubtract(center, XMVectorScale(L, sceneRadius + 50.0f));

	// 光线和目标点的连线不能与世界up平行
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	if (fabsf(XMVectorGetY(L)) > 0.99f) up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	//正交投影和观察矩阵
	XMMATRIX view = XMMatrixLookAtLH(eye, center, up);
	XMMATRIX proj = XMMatrixOrthographicOffCenterLH(
		-sceneRadius, sceneRadius, -sceneRadius, sceneRadius, 1.0f, depth);
	XMMATRIX viewProj = view * proj;

	//填充阴影pass
	m_shadowPassCB = mainPass;
	XMStoreFloat4x4(&m_shadowPassCB.view, XMMatrixTranspose(view));
	XMStoreFloat4x4(&m_shadowPassCB.proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&m_shadowPassCB.viewProj, XMMatrixTranspose(viewProj));
	m_shadowPassCB.renderTargetSize = XMFLOAT2((float)m_size, (float)m_size);
	m_shadowPassCB.invRenderTargetSize = XMFLOAT2(1.0f / m_size, 1.0f / m_size);
	m_shadowPassCB.isClipPlane = 0; // 阴影Pass不裁剪

	XMStoreFloat4x4(&m_shadowPassCB.shadowTransform, XMMatrixTranspose(viewProj));
}

void ShadowMap::BuildResources()
{
	// 建立纹理资源
	// 作为深度图(光源方向深度)和着色器资源(用于着色器采样)

	//F32
	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R32_TYPELESS, m_size, m_size, 1, 1, 1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
		&defaultHeap, D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, //初始设为着色器可访问
		&clearValue,
		IID_PPV_ARGS(&m_resource)));


}

void ShadowMap::BuildDescriptors()
{
	//SRV:同一资源也可以描述为着色器资源
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	m_d3dDevice->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_hCpuSrv);

	// DSV:同一资源也可以描述为深度资源
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	m_d3dDevice->CreateDepthStencilView(m_resource.Get(), &dsvDesc, m_hCpuDsv);
}