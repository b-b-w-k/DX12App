#pragma once

#include"D3DTools.h"
#include"GameTimer.h"//计时器

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

/*
	应用程序基类(需继承实现)

*/
class GameApp
{
protected:
	GameApp(HINSTANCE hInstance);
	GameApp(const GameApp&) = delete;
	GameApp& operator=(const GameApp&) = delete;

	//重写
	virtual ~GameApp();
public:
	static GameApp* GetInstance();    //本类实例
	HINSTANCE       GetAppInst();//初始化句柄
	HWND            GetMainWnd();//入口句柄

	float AspectRatio(); //宽高比
	bool  GetMSAAState();//4xMsaa是否开启
	void  SetMSAAState(bool isOpen);
	
	int GameRun();//运行应用程序

	//=======重写=======
	virtual bool InitApp();
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);//窗口消息处理函数

protected:
	//=======重写=======
	virtual void CreateRtvAndDsvDescHeaps();//创建Rtv和Dsv堆
	virtual void OnResize();//更改窗口大小
	virtual void GameUpdate()=0;//更新应用程序
	virtual void GameRender()=0;//渲染应用程序



protected:
	bool InitWindow();//初始化窗口
	bool InitD3D();//Direct3D初始化
	void CreateCmdObjects();//创建命令对象(列表,队列,分配器)
	void CreateSwapChain();//创建交换链
	void CreateMsaaResources();//创建MSAA资源
	
	void FinishCmdQue();//等待命令队列执行完成
	void ShowFrameRate();//显示帧率

	ID3D12Resource* GetCurrentBackBuffer()const;//当前后台缓冲区
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferView()const;//当前后台缓冲区视图
	D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView()const;//当前深度/模板缓冲区视图



protected:
	static GameApp* m_appInst;//本类实例

	HINSTANCE m_hAppInst = nullptr;    //初始化句柄
	HWND      m_hWnd = nullptr;        //窗口句柄
	bool      m_isAppPause = false;    //是否暂停
	bool      m_isMax = false;         //是否最大化
	bool      m_isMin = false;         //是否最小化
	bool      m_isCaptureMouse = false;//是否捕获鼠标
	bool      m_isResizing = false;    //尺寸是否发生变化
	bool      m_isFinishCmdQue = false;//等待命令队列
	bool      m_is4xMsaaState = false; //启用4xMSAA
	UINT      m_4xMsaaQuality = 0;     //4xMSAA质量等级

	GameTimer m_timer;//计时器

	//DX接口
	Microsoft::WRL::ComPtr<IDXGIFactory4>  m_dxgiFactory;//dxgi工厂
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;  //交换链
	Microsoft::WRL::ComPtr<ID3D12Device>   m_d3dDevice;  //设备

	//
	//
	//围栏
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;//围栏值
	UINT64 m_currentFence = 0;

	//命令队列
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>        m_commandQueue;//命令队列
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    m_commandAllocator;//命令分配器
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;//命令列表
	
	//MSAA的缓冲区
	Microsoft::WRL::ComPtr<ID3D12Resource>       m_msaaRenderTarget;	//msaa的RT资源(渲染目标)
	Microsoft::WRL::ComPtr<ID3D12Resource>	     m_msaaDepthStencil;	//msaa的DS资源(深度/模板缓冲区)
	

	//后台缓冲区/深度/模板缓冲区
	static const int m_SCBCount = 2;//后台缓冲区数量(双重缓冲)
	int m_currBackBuffer = 0;//后台缓冲区索引
	Microsoft::WRL::ComPtr<ID3D12Resource> m_swapChainBuffer[m_SCBCount]; //后台缓冲区列表
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;//深度/模板缓冲区

	//描述符堆
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RtvHeap;//渲染目标视图(描述符堆)
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DsvHeap;//深度/模板缓冲区视图(描述符堆)

	D3D12_VIEWPORT m_screenViewport; //屏幕视口
	D3D12_RECT m_scissorRect;//裁切矩形

	//视图大小
	UINT m_RtvDescriptorSize = 0;//渲染目标视图(描述符)大小
	UINT m_DsvDescriptorSize = 0;///深度/模板缓冲区视图(描述符)大小
	UINT m_CbvSrvUavDescriptorSize = 0;//常量/着色器/无序访问视图大小
	
	
	WORD m_width = 800;//窗口宽度
	WORD m_height = 600;//窗口高度
	std::wstring m_titelName = L"DX12App";//窗口标题
	std::wstring m_className = L"WndClass";//窗口类名
	D3D_DRIVER_TYPE m_d3dDriverType = D3D_DRIVER_TYPE_HARDWARE;//枚举常量 默认为以来独显加速
	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;//后台缓冲区格式
	DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; //深度 / 模板缓冲区格式

};

