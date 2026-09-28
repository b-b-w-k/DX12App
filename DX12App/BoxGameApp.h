#pragma once
#include <vector>
#include <unordered_map>
#include"GameApp.h"
#include"Camera.h"
#include"FrameResource.h"
#include "d3dx12.h"



#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

class InputManage;
class ICameraController;
class D2DTextRenderer;
class Scene;
class SceneObject;
class DebugLineRenderer;
class ReflectionRTT;


using  namespace Microsoft::WRL;

class BoxGameApp
    :public GameApp
{
public:
    BoxGameApp(HINSTANCE hInstance);
    BoxGameApp(const BoxGameApp& rhs) = delete;
    BoxGameApp& operator=(const BoxGameApp& rhs) = delete;
    ~BoxGameApp();

    bool InitApp()override;
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)override;
private:
    void GameUpdate() override;
    void GameRender() override;
    void OnResize()override;

	void DrawReflections();            // 绘制反射纹理(平面/立体)
	void DrawD2DText();                // 绘制2D文字
	void DrawDebugLine();              // 绘制调试线

	void UpdateController(float dt);   // 更新摄像机控制器
	void UpdateMainPassCB(float dt);   // 更新主Pass常量缓冲区

    void BuildShadersAndInputLayout(); // 着色器编译及输入布局描述
    void BuildRootSignature();         // 构建根签名
    void BuildFrameResource();         // 构建帧资源
    void BuildPSO();                   // 构建渲染管线状态对象
    void BuildScene();                 // 构建场景
    void BuildReflection();            // 构建反射

    void BuildGlobalDescriptorHeaps();           // 建立全局描述符堆
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocSrvCpu(); // 分配SRV描述符句柄CPU
	CD3DX12_GPU_DESCRIPTOR_HANDLE AllocSrvGpu(); // 分配SRV描述符句柄GPU
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocRtvCpu(); // 分配RTV描述符句柄CPU
	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocDsvCpu(); // 分配DSV描述符句柄CPU


    //构建静态采样器
    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
    
    //世界转屏幕坐标
    bool WorldToScreen(const DirectX::XMFLOAT3& world, DirectX::XMFLOAT2& outScreen);
    
private:

    // ====== 帧资源 ======
    std::vector<std::unique_ptr<FrameResource>> m_frameResources;       // 帧资源
    FrameResource* m_curFrameResource = nullptr;                        // 帧资源指针
    int m_curFrameResourceIndex = 0;                                    // 当前帧资源索引

	// ===== 特殊渲染器 ===== 
	std::unique_ptr<D2DTextRenderer>   m_textRenderer  = nullptr;       // 文字绘制接口
	std::unique_ptr<DebugLineRenderer> m_debugRenderer = nullptr;       // 调试线绘制接口

	// ====== 交互管理器 ======
	std::unique_ptr<InputManage>       m_inputManage  = nullptr;        // 输入管理
	std::unique_ptr<ICameraController> m_camControl   = nullptr;        // 摄像机控制器
	
    // ===== 管线资源 =====
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;                // 标准输入布局描述
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_colorInputLayout;           // 纯色输入布局描述
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_skinInputLayout;            // 蒙皮输入布局描述
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;              // 根签名
    std::unordered_map<std::string, ComPtr<ID3DBlob>> m_shaders;        // 着色器
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;// 渲染管线状态对象
    
    
	// ===== 全局描述符堆(不包含初始化所用的描述符) =====                                        
	ComPtr<ID3D12DescriptorHeap> m_globalSrvHeap;                       // 全局着色器资源视图堆
	ComPtr<ID3D12DescriptorHeap> m_globalRtvHeap;                       // 全局渲染目标视图堆
	ComPtr<ID3D12DescriptorHeap> m_globalDsvHeap;                       // 全局深度模板视图堆

	UINT m_srvSlotCount   = 0;                                          // 已用槽位计数
	UINT m_rtvSlotCount   = 0;
	UINT m_dsvSlotCount   = 0;

    UINT m_cbvSrvDescSize = 0;                                          // 描述符大小
	UINT m_rtvDescSize    = 0;                                          
	UINT m_dsvDescSize    = 0;


    // ===== 场景,摄像机 =====
    std::unique_ptr<Scene> m_scene = nullptr;                           // 场景(管理场景对象)
    Camera m_camera;                                                    // 摄像机        


	// ===== 反射 =====
	std::unique_ptr<ReflectionRTT>     m_waterRT;                       // 水面反射(平面)用于构建资源,更新常量缓冲区
	std::unique_ptr<ReflectionRTT>     m_envRT;                         // 环境反射(立体)于构建资源,更新常量缓冲区



    // ===== 过程常量缓冲区更新数据 =====
	PassConstants m_mainPassCB;                                         // 主过程常量缓冲区
	PassConstants m_reflectPassCB;                                      // 反射过程常量缓冲区

	bool m_isShow = false;                                              // 是否显示调试信息


};

