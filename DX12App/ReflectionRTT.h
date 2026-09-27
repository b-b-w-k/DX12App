#pragma once
#include "FrameResource.h"


// 构建反射资源的格式
enum class EReflectType
{
	PLANE, // 平面
	CUBE,  // 立方体
};

//建立反射渲染到纹理需要的资源和更新反射常量缓冲区
class ReflectionRTT
{
public:
	ReflectionRTT(ID3D12Device* device,
				  EReflectType type,
				  UINT width, UINT height,
				  DXGI_FORMAT RTFormat,
				  DXGI_FORMAT DSFormat);
	ReflectionRTT(const ReflectionRTT&) = delete;
	ReflectionRTT& operator=(const ReflectionRTT&) = delete;
	~ReflectionRTT() = default;

	// 建立描述符(堆由外部传入,本类只负责创建资源和描述符)
	void BuildDesccriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,    // 着色器资源句柄CPU
						   CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,    // 着色器资源句柄GPU
						   CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtvBase,// 渲染目标视图开始句柄
						   UINT rtvDescriptorSize,                   // 渲染目标视图大小
						   CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);   // 深度缓冲区句柄

	// 设置更新所用信息
	void SetPlane(const DirectX::XMFLOAT4& plane) { m_plane = plane; }       // 设置反射平面
	void SetCapturePos(const DirectX::XMFLOAT3& pos) { m_capturePos = pos; } // 设置反射探针位置

	//更新
	void UpdateFromMainPass(const PassConstants& mainPass); //通过主Pass更新反射PassCB,方便填充

	// 查询成员
	// 获取反射纹理类型
	EReflectType GetType()const { return m_type; }
	bool IsCube()const {return m_type == EReflectType::CUBE;}
	// 获取视图数量(平面1,立方体6)
	UINT GetFaceCount() const { return m_type == EReflectType::CUBE ? 6u : 1u; }

	ID3D12Resource* GetResource() { return m_resource.Get(); }   // SRV/RTV所描述的资源
	ID3D12Resource* GetDepthResource() { return m_depth.Get(); } // DSV所描述的资源

	CD3DX12_GPU_DESCRIPTOR_HANDLE GetHGpuSrv() { return m_hGpuSrv; } //GPU资源用于着色器访问
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetHCpuDsv() { return m_hCpuDsv; } //CPU资源用于渲染目标绑定
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetHCpuRtv(UINT faceIndex = 0)     //CPU资源用于渲染目标绑定
	{
		assert(faceIndex <= GetFaceCount());
		return m_hCpuRtvs[faceIndex];
	}

	// 获取反射常量缓冲区
	const PassConstants& GetRefPassCB(UINT face = 0)const { return m_refPassCBs[face]; }

	D3D12_VIEWPORT  GetViewport() const { return m_viewport; }
	D3D12_RECT  GetScreenRect() const { return m_screenRect; }

	UINT GetWidth()const { return m_width; }
	UINT GetHeight()const { return m_height; }

	void OnResize(UINT newWidth, UINT newHeight);

private:
	void BuildDescriptors(); // 建立描述符
	void BuildResource();    // 建立资源

	//计算反射常量缓冲区
	void BuildPlanePassCB(const PassConstants& mainPass);
	void BuildFacePassCB(UINT face, const PassConstants& mainPass);

private:
	ID3D12Device* m_d3dDevice = nullptr;
	EReflectType m_type;

	// 视口和裁剪矩形
	D3D12_VIEWPORT m_viewport = {};
	D3D12_RECT m_screenRect = {};
	// 资源宽高
	UINT m_width = 0;
	UINT m_height = 0;

	// 资源格式
	DXGI_FORMAT m_rtFormat = DXGI_FORMAT_R8G8B8A8_UNORM;    // 渲染目标格式
	DXGI_FORMAT m_dsFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度模板格式

	// 描述符句柄
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE m_hGpuSrv;      // SRV GPU端句柄用于着色器访问
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuRtvs[6];   
	CD3DX12_CPU_DESCRIPTOR_HANDLE m_hCpuDsv;

	// 资源
	Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depth;

	// 反射常量缓冲区更新所用信息
	DirectX::XMFLOAT4 m_plane = { 0.f, 1.f, 0.f, 0.f };			// 平面(Plane)
	DirectX::XMFLOAT3 m_capturePos = { 0.f, 0.f, 0.f };			// 捕获位置(Cube)
	PassConstants m_refPassCBs[6];						// 反射常量缓冲区,平面用0号位
};

