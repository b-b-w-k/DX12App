#include "ReflectionRTT.h"

using namespace DirectX;
ReflectionRTT::ReflectionRTT(ID3D12Device* device, 
						   EReflectType type, 
						   UINT width, UINT height, 
						   DXGI_FORMAT RTFormat, 
						   DXGI_FORMAT DSFormat)
	:m_d3dDevice(device)
	,m_type(type)
	,m_width(width)
	,m_height(height)
	,m_rtFormat(RTFormat)
	,m_dsFormat(DSFormat)
{
	m_viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	m_screenRect = { 0, 0, (LONG)width, (LONG)height };

	BuildResource();
}

void ReflectionRTT::BuildDesccriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, 
									 CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, 
									 CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtvBase, 
									 UINT rtvDescriptorSize, 
									 CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
	//缓存句柄
	m_hCpuSrv = hCpuSrv;
	m_hGpuSrv = hGpuSrv;
	m_hCpuDsv = hCpuDsv;

	for (UINT i=0;i<6;++i)
	{
		m_hCpuRtvs[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(
			hCpuRtvBase, i, rtvDescriptorSize);
	}
	BuildDescriptors();
}

void ReflectionRTT::UpdateFromMainPass(const PassConstants& mainPass)
{
	//区分平面与立方体
	if (IsCube())
		for (UINT i = 0; i < 6; ++i)
			BuildFacePassCB(i, mainPass);
	else
		BuildPlanePassCB(mainPass);
}

void ReflectionRTT::BuildPlanePassCB(const PassConstants& mainPass)
{
	//计算反射矩阵
	XMVECTOR planeV = XMLoadFloat4(&m_plane);
	XMMATRIX R = XMMatrixReflect(planeV);

	XMMATRIX V = XMMatrixTranspose(XMLoadFloat4x4(&mainPass.view));
	XMMATRIX P = XMMatrixTranspose(XMLoadFloat4x4(&mainPass.proj));

	//计算镜像的观察矩阵
	XMMATRIX RV = R * V;
	XMMATRIX RPV = RV * P;

	//填充平面反射常量缓冲区
	PassConstants& cb = m_refPassCBs[0];
	cb = mainPass;
	XMStoreFloat4x4(&cb.view, XMMatrixTranspose(RV));
	XMStoreFloat4x4(&cb.viewProj, XMMatrixTranspose(RPV));

	//镜像视线方向
	XMVECTOR eye = XMLoadFloat3(&mainPass.eyePosW);
	XMStoreFloat3(&cb.eyePosW, XMVector3TransformCoord(eye, R));

	auto det = XMMatrixDeterminant(RV);
	XMStoreFloat4x4(&cb.invView,
		XMMatrixTranspose(XMMatrixInverse(&det, RV)));
	det = XMMatrixDeterminant(RPV);
	XMStoreFloat4x4(&cb.invViewProj,
		XMMatrixTranspose(XMMatrixInverse(&det, RPV)));

	//更新裁剪平面
	XMVECTOR clipPlane = XMLoadFloat4(&m_plane);
	XMStoreFloat4(&cb.clipPlane, clipPlane);
	cb.isClipPlane = 1;//启用裁剪平面


}

void ReflectionRTT::BuildFacePassCB(UINT face, const PassConstants& mainPass)
{

	//6个方向的观察方向
	static const XMFLOAT3 s_lookAt[6] =
	{
		{  1,  0,  0 }, { -1,  0,  0 },
		{  0,  1,  0 }, {  0, -1,  0 },
		{  0,  0,  1 }, {  0,  0, -1 },
	};
	//更改上向量
	static const XMFLOAT3 s_up[6] =
	{
	   { 0, 1, 0  },  { 0, 1, 0 },
	   { 0, 0, -1 },  { 0, 0, 1 },
	   { 0, 1, 0  },  { 0, 1, 0 },
	};

	XMVECTOR eye = XMLoadFloat3(&m_capturePos);
	XMVECTOR target = XMVectorAdd(eye, XMLoadFloat3(&s_lookAt[face]));
	XMVECTOR up = XMLoadFloat3(&s_up[face]);

	//计算观察矩阵
	XMMATRIX V = XMMatrixLookAtLH(eye, target, up);
	XMMATRIX P = XMMatrixPerspectiveFovLH(
		XM_PIDIV2, 1.0f, mainPass.nearZ, mainPass.farZ);

	//填充立方体过程常量缓冲区
	PassConstants& cb = m_refPassCBs[face];
	cb = mainPass;
	XMStoreFloat4x4(&cb.view, XMMatrixTranspose(V));
	XMStoreFloat4x4(&cb.proj, XMMatrixTranspose(P));
	XMStoreFloat4x4(&cb.viewProj, XMMatrixTranspose(V * P));
	XMStoreFloat3(&cb.eyePosW, eye);

	auto det = XMMatrixDeterminant(V);
	XMStoreFloat4x4(&cb.invView,
		XMMatrixTranspose(XMMatrixInverse(&det, V)));
	det = XMMatrixDeterminant(P);
	XMStoreFloat4x4(&cb.invProj,
		XMMatrixTranspose(XMMatrixInverse(&det, P)));
	det = XMMatrixDeterminant(V * P);
	XMStoreFloat4x4(&cb.invViewProj,
		XMMatrixTranspose(XMMatrixInverse(&det, V * P)));

	cb.isClipPlane = 0;//不启用裁剪平面
}

void ReflectionRTT::OnResize(UINT newWidth, UINT newHeight)
{
	if (m_width == newWidth && m_height == newHeight)return;

	m_height = newHeight;
	m_width = newWidth;

	m_viewport = { 0.0f, 0.0f, (float)newWidth, (float)newHeight, 0.0f, 1.0f };
	m_screenRect = { 0, 0, (LONG)newWidth, (LONG)newHeight };

	//重新场景资源和描述符
	BuildResource();
	BuildDescriptors();
}

void ReflectionRTT::BuildDescriptors()
{
	//========建立着色器资源视图SRV========
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (IsCube())
	{
		//立方体纹理
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = 1;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	}
	else
	{
		//单一2D纹理
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	}
	// 创建资源视图
	m_d3dDevice->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_hCpuSrv);

	//========创建渲染目标视图RTV========

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = m_rtFormat;

	if (IsCube())
	{
		//Cube有6个面每个都要创建RTV
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.ArraySize = 1;

		//6个RTV
		for (UINT i = 0; i < 6; i++)
		{
			rtvDesc.Texture2DArray.FirstArraySlice = i;
			m_d3dDevice->CreateRenderTargetView(
				m_resource.Get(), &rtvDesc, m_hCpuRtvs[i]);
		}
	}
	else
	{
		//平面只要一个RTV
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;
		m_d3dDevice->CreateRenderTargetView(
			m_resource.Get(), &rtvDesc, m_hCpuRtvs[0]);//默认以0号位为准

	}

	//========创建深度模板缓冲区DSV========

	//两者都只需要一个
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = m_dsFormat;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	m_d3dDevice->CreateDepthStencilView(m_depth.Get(), &dsvDesc, m_hCpuDsv);
}

void ReflectionRTT::BuildResource()
{
	auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	//=======Rt资源=======
	D3D12_RESOURCE_DESC rtDesc = {};
	rtDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//都是2d纹理
	rtDesc.Width = m_width;
	rtDesc.Height = m_height;
	rtDesc.MipLevels = 1;
	rtDesc.Format = m_rtFormat;
	rtDesc.SampleDesc.Count = 1;
	rtDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;//渲染目标

	//区分Cube和Plane
	rtDesc.DepthOrArraySize = IsCube() ? 6 : 1; //Cube使用6层的纹理数组

	//设置清理值
	D3D12_CLEAR_VALUE rtClear = {};
	rtClear.Format = m_rtFormat;
	rtClear.Color[0] = 0.0f;
	rtClear.Color[1] = 0.0f;
	rtClear.Color[2] = 0.0f;
	rtClear.Color[3] = 1.0f;

	ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
		&defaultHeap, D3D12_HEAP_FLAG_NONE,
		&rtDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&rtClear,
		IID_PPV_ARGS(&m_resource)));

	//=======DS资源=======
	D3D12_RESOURCE_DESC dsDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		m_dsFormat, m_width, m_height, 1, 1, 1, 0,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
	//设置清理值
	D3D12_CLEAR_VALUE dsClear = {};
	dsClear.Format = m_dsFormat;
	dsClear.DepthStencil.Depth = 1.0f;
	dsClear.DepthStencil.Stencil = 0;

	ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
		&defaultHeap, D3D12_HEAP_FLAG_NONE,
		&dsDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&dsClear,
		IID_PPV_ARGS(&m_depth)));

}
