#include "PCH.h"
#include "GameApp.h"

//静态实例声明
GameApp* GameApp::m_appInst = nullptr;

LRESULT CALLBACK 
WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return GameApp::GetInstance()->MsgProc(hwnd, msg, wParam, lParam);
}

//传入win32初始化句柄
GameApp::GameApp(HINSTANCE hInstance)
	:m_hAppInst(hInstance)
{
	assert(m_appInst == nullptr);//断言只有一个实例才继续
	m_appInst = this;
}

GameApp::~GameApp()
{
	//等待命令队列执行完成才释放
	if (m_d3dDevice != nullptr)
	{
		FinishCmdQue();
	}
}

GameApp* GameApp::GetInstance()
{
	return m_appInst;
}

HINSTANCE GameApp::GetAppInst()
{
	return m_hAppInst;
}

HWND GameApp::GetMainWnd()
{
	return m_hWnd;
}

float GameApp::AspectRatio()
{
	return static_cast<float>(m_width) / m_height;
}

bool GameApp::GetMSAAState()
{
	return m_is4xMsaaState;
}

void GameApp::SetMSAAState(bool isOpen)
{
	if (m_is4xMsaaState != isOpen)
	{
		m_is4xMsaaState = isOpen;
		OnResize();//重新加载
	}
}

bool GameApp::InitApp()
{
	if (!InitWindow()) return false;// 窗口初始化
	if (!InitD3D())    return false;//D3D初始化
	// 刷新屏幕
	OnResize();

	return true;
}

bool GameApp::InitWindow()
{


	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = m_hAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION); //使用默认应用程序图标
	//wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)); //使用资源文件中的图标
	wc.hCursor = LoadCursor(0, IDC_ARROW); //使用默认箭头光标
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); //使用黑色画刷作为背景

	wc.lpszMenuName = 0;
	wc.lpszClassName = m_className.c_str();
	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}
	//还要除去菜单栏等尺寸
	RECT rect = { 0, 0, m_width, m_height };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;

	m_hWnd = CreateWindow(m_className.c_str(), m_titelName.c_str(),WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, m_hAppInst, 0);
	if (!m_hWnd)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}
	ShowWindow(m_hWnd, SW_SHOW);
	UpdateWindow(m_hWnd);
	return true;
}

bool GameApp::InitD3D()
{
	//创建工厂
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)));

	//创建显卡设备
	ThrowIfFailed(D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&m_d3dDevice)));

	//(可选)失败降级到虚拟显卡设备
	//...

	//创建围栏
	ThrowIfFailed(m_d3dDevice->CreateFence(
		0, 
		D3D12_FENCE_FLAG_NONE, 
		IID_PPV_ARGS(&m_fence)));

	//获取描述符大小(Rtv,Dsv,Uav)
	m_RtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_DsvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_CbvSrvUavDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//检查4xMsaa支持情况
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels;
	qualityLevels.Format = m_backBufferFormat;
	qualityLevels.SampleCount = 4;
	qualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	qualityLevels.NumQualityLevels = 0;

	ThrowIfFailed(m_d3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&qualityLevels,
		sizeof(qualityLevels)));
	m_4xMsaaQuality = qualityLevels.NumQualityLevels;
	//断言
	assert(m_4xMsaaQuality > 0);

	CreateCmdObjects();//命令对象
	CreateSwapChain();//交换链
	CreateRtvAndDsvDescHeaps();//描述符堆

	//具体资源调OnResize创建

	return true;
}

void GameApp::OnResize()
{
	assert(m_d3dDevice);//dx设备
	assert(m_swapChain);//交换链
	assert(m_commandAllocator);//命令分配器

	FinishCmdQue();//等待命令完成才能重新加载

	//重新设置命令列表
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

	//释放(前)后台缓冲区中的资源
	for (int i = 0; i < m_SCBCount; ++i)
	{
		m_swapChainBuffer[i].Reset();
		
	}
	//释放Ds缓冲区中的资源
	m_depthStencilBuffer.Reset();
	//开启Msaa的Dsv资源
	m_msaaRenderTarget.Reset();
	m_msaaDepthStencil.Reset();   //新增这两行

	///重新设置交换链中的资源
	ThrowIfFailed(m_swapChain->ResizeBuffers(
		m_SCBCount,
		m_width, m_height,
		m_backBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
	m_currBackBuffer = 0;



	//为交换链的每个后台缓冲区创建渲染目标视图(Rtv)
	// 
	//获取Rtv堆起始句柄
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(
		m_RtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < m_SCBCount; i++)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(i,
			IID_PPV_ARGS(&m_swapChainBuffer[i])));
		//将Rtv视图存入堆中
		m_d3dDevice->CreateRenderTargetView(
			m_swapChainBuffer[i].Get(),
			nullptr,
			rtvHeapHandle);
		//偏移堆
		rtvHeapHandle.Offset(1, m_RtvDescriptorSize);
	}

	if (!m_is4xMsaaState)//没有Msaa就创建,因为msaa会创建Dsv
	{
		//创建深度/模板缓冲区及其视图(描述符)
		//CD3DX12_RESOURCE_DESC::Tex2D自动填充内部参数
		D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			m_depthStencilFormat,
			m_width,
			m_height,
			1, 1, 1, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);//深度/模板缓冲区标志

		//清理视图设置
		D3D12_CLEAR_VALUE optClear{};
		optClear.Format = m_depthStencilFormat;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;
		
		auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
			reinterpret_cast<const D3D12_HEAP_PROPERTIES*>(&heap),
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&optClear,
			IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

		// 创建深度/模板缓冲区视图
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = m_depthStencilFormat;
		dsvDesc.Texture2D.MipSlice = 0;
		m_d3dDevice->CreateDepthStencilView(
			m_depthStencilBuffer.Get(),
			&dsvDesc,
			GetDepthStencilView());

		//更改资源呈现状态
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_depthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_COMMON,//默认
			D3D12_RESOURCE_STATE_DEPTH_WRITE);

		m_commandList->ResourceBarrier(1, &barrier);//深度写入
	}
	else
	{
		//创建MSAA
		CreateMsaaResources();

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_msaaDepthStencil.Get(),
			D3D12_RESOURCE_STATE_COMMON,//默认
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
		//更改资源呈现状态
		m_commandList->ResourceBarrier(1, &barrier);//深度写入
	}
	

	//关闭命令列表
	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 等待命令队列执行完毕
	FinishCmdQue();

	//更新视口
	m_screenViewport.TopLeftX = 0;
	m_screenViewport.TopLeftY = 0;
	m_screenViewport.Width = static_cast<float>(m_width);
	m_screenViewport.Height = static_cast<float>(m_height);
	m_screenViewport.MinDepth = 0.0f;
	m_screenViewport.MaxDepth = 1.0f;

	//裁切矩形
	m_scissorRect = { 0,0,m_width ,m_height };
}

int GameApp::GameRun()
{
	MSG msg = { 0 };

	m_timer.Reset();

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			if (!m_isAppPause)
			{
				//更新计时器
				m_timer.Tick();
				//展示帧率
				ShowFrameRate();
				//更新
				GameUpdate();
				//渲染
				GameRender();
			}
			else
			{
				Sleep(100);
			}
			

		}
	}
	return (int)msg.wParam;
}

LRESULT GameApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		//检测窗口是否激活,如果没有激活就暂停游戏
	case WM_ACTIVATE:

		if (LOWORD(wParam) == WA_INACTIVE)
		{
			m_isAppPause = true;
			m_timer.Pause();
		}
		else
		{
			m_isAppPause = false;
			m_timer.Start();
		}
		return 0;
		//大小调整
	case WM_SIZE:
		//更新宽高
		m_width = LOWORD(lParam);
		m_height = HIWORD(lParam);
		if (m_d3dDevice)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				//暂停
				m_isAppPause = true;
				m_isMax = false;
				m_isMin = true;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				m_isAppPause = false;
				m_isMax = true;
				m_isMin = false;
				
				//重新设置视口
				OnResize();

			}
			else if (wParam == SIZE_RESTORED)
			{
				if (m_isMin)
				{
					m_isMin = false;
					m_isAppPause = false;
					//重新设置视口
					OnResize();
				}
				else if (m_isMax)
				{
					m_isMax = false;
					m_isAppPause = false;
					//重新设置视口
					OnResize();
				}
				else if (m_isResizing)
				{
					/* 
					在WM_EXITSIZEMOVE处调整
					*/
				}
				else
				{
					//重新设置视口
					OnResize();
				}
			}

		}
		return 0;
		//拖动或拉伸边框开始
	case WM_ENTERSIZEMOVE:
		m_isAppPause = true;
		m_isResizing = true;
		m_timer.Pause();
		return 0;
		//拖动或拉伸边框结束
	case WM_EXITSIZEMOVE:
		m_isAppPause = false;
		m_isResizing = false;
		m_timer.Start();
		OnResize();
		return 0;
		//设置最小/最大窗口
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;
		//关闭窗口
	case WM_CLOSE:
		DestroyWindow(m_hWnd);
		return 0;
		//鼠标位置
	case WM_MOUSEMOVE:
		return 0;
		// 鼠标按下(鼠标需传入虚拟键码)
	case WM_LBUTTONDOWN://左键

		return 0;
	case WM_RBUTTONDOWN://右键

		return 0;
		// 鼠标抬起
	case WM_LBUTTONUP://左键

		return 0;
	case WM_RBUTTONUP://右键

		return 0;
		//键盘按下
	case WM_KEYDOWN:

		return 0;
		//键盘抬起
	case WM_KEYUP:

		return 0;
	case WM_DESTROY:
		
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void GameApp::CreateRtvAndDsvDescHeaps()
{
	//创建rtv堆
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = m_SCBCount + 1;//两个后台+Msaa
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(m_RtvHeap.GetAddressOf())));

	//创建dsv堆
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(m_DsvHeap.GetAddressOf())));

}

void GameApp::CreateCmdObjects()
{
	//创建命令队列
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(m_d3dDevice->CreateCommandQueue(
		&cmdQueueDesc, IID_PPV_ARGS(m_commandQueue.GetAddressOf())));

	//创建命令分配器
	ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, 
		IID_PPV_ARGS(m_commandAllocator.GetAddressOf())));

	//创建命令列表
	ThrowIfFailed(m_d3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_commandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(m_commandList.GetAddressOf())));

	m_commandList->Close();

}

void GameApp::CreateSwapChain()
{
	m_swapChain.Reset();

	//交换链的描述 
	// Direct12不支持直接在后台缓冲区中启用MSAA
	//启用MSAA需要创建等待的MSAA资源
	DXGI_SWAP_CHAIN_DESC scDesc;
	scDesc.BufferDesc.Width = m_width;
	scDesc.BufferDesc.Height = m_height;
	scDesc.BufferDesc.RefreshRate.Numerator = 60;
	scDesc.BufferDesc.RefreshRate.Denominator = 1;
	scDesc.BufferDesc.Format = m_backBufferFormat;
	scDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	//在msaa里设置======
	scDesc.SampleDesc.Count = 1;
	scDesc.SampleDesc.Quality = 0;
	//==================
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;//允许rtv绑定
	scDesc.BufferCount = m_SCBCount;
	scDesc.OutputWindow = m_hWnd;
	scDesc.Windowed = true;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//flip模型
	scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(m_dxgiFactory->CreateSwapChain(
		m_commandQueue.Get(),
		&scDesc,
		m_swapChain.GetAddressOf()));

}

void GameApp::CreateMsaaResources()
{
	if (m_is4xMsaaState)//开启
	{
		//堆默认参数
		D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		//创建msaa渲染目标缓冲区
		D3D12_RESOURCE_DESC msaaRtDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			m_backBufferFormat,
			m_width,
			m_height,
			1, 1, 4, 0,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

		D3D12_CLEAR_VALUE msaaRtOptClear = {};//ds数量0
		msaaRtOptClear.Format = m_backBufferFormat;
		msaaRtOptClear.Color[0] = DirectX::Colors::LightSteelBlue.f[0];
		msaaRtOptClear.Color[1] = DirectX::Colors::LightSteelBlue.f[1];
		msaaRtOptClear.Color[2] = DirectX::Colors::LightSteelBlue.f[2];
		msaaRtOptClear.Color[3] = DirectX::Colors::LightSteelBlue.f[3];

		
		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&msaaRtDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&msaaRtOptClear,
			IID_PPV_ARGS(m_msaaRenderTarget.ReleaseAndGetAddressOf())));

		m_msaaRenderTarget->SetName(L"MSAA Render Target");
		//建msaa渲染目标视图
		D3D12_RENDER_TARGET_VIEW_DESC msaaRtvDesc = {};
		msaaRtvDesc.Format = m_backBufferFormat;
		msaaRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;//MSAA专用

		m_d3dDevice->CreateRenderTargetView(
			m_msaaRenderTarget.Get(),
			&msaaRtvDesc,
			CD3DX12_CPU_DESCRIPTOR_HANDLE(
				m_RtvHeap->GetCPUDescriptorHandleForHeapStart(),
				m_SCBCount,//偏移交换链后台缓冲区数量
				m_RtvDescriptorSize));
		
		//创建Ds缓冲区
		D3D12_RESOURCE_DESC msaaDsDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			m_depthStencilFormat,
			m_width,
			m_height,
			1, 1, 4, 0,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		D3D12_CLEAR_VALUE msaaDsOptClear = {};
		msaaDsOptClear.Format = m_depthStencilFormat;
		msaaDsOptClear.DepthStencil.Depth = 1.0f;
		msaaDsOptClear.DepthStencil.Stencil = 0;

		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&msaaDsDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&msaaDsOptClear,
			IID_PPV_ARGS(m_msaaDepthStencil.ReleaseAndGetAddressOf())));

		//创建Dsv视图
		D3D12_DEPTH_STENCIL_VIEW_DESC msaaDsvDesc = {};
		msaaDsvDesc.Format = m_depthStencilFormat;
		msaaDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;//MSAA专用

		m_d3dDevice->CreateDepthStencilView(
			m_msaaDepthStencil.Get(),
			&msaaDsvDesc,
			GetDepthStencilView());
	}
	else
	{
		// 释放msaa资源，不创建MSAA资源
		m_msaaRenderTarget.Reset();
		m_msaaDepthStencil.Reset();
	}
	
}

void GameApp::FinishCmdQue()
{
	//推进围栏值，以标记截至该围栏点之前的所有命令。
	m_currentFence++;

	// 设置新的围栏值(要等命令队列把前面的命令执行完才会设置)
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFence));

	if (m_fence->GetCompletedValue() < m_currentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
		//发布事件
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));
		//订阅事件(等待)
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

ID3D12Resource* GameApp::GetCurrentBackBuffer() const
{
	return m_swapChainBuffer[m_currBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE GameApp::GetCurrentBackBufferView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
	m_RtvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_currBackBuffer,
		m_RtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE GameApp::GetDepthStencilView() const
{
	return  m_DsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void GameApp::ShowFrameRate()
{
	//帧
	static int fpsCount = 0;
	//已用时间
	static float timeElapsed = 0.0f;

	fpsCount++;

	//如果过了1秒就执行
	if (m_timer.TotalTime()- timeElapsed>=1.0f)
	{
		float fps = (float)fpsCount;
		float mfps = 1000.0f / fps;

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mfpsStr = std::to_wstring(mfps);

		std::wstring winStr = m_titelName
			+ L"    fps:" + fpsStr
			+ L"    mfps:" + mfpsStr;

		SetWindowText(m_hWnd, winStr.c_str());
		fpsCount = 0;
		timeElapsed += 1.0f;
	}

}


