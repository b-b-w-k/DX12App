#include "PCH.h"
#include "D2DTextRenderer.h"


#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d3d11.lib")

D2DTextRenderer::~D2DTextRenderer()
{
	ReleaseSwapChainResources();
}

bool D2DTextRenderer::InitD2DRender(ID3D12Device* d3d12Device, 
									ID3D12CommandQueue* cmdQueue, 
									IDXGISwapChain* swapChain, 
									UINT backBufferCount)
{
	if (m_isInitialized) return true;
	if (!d3d12Device || !cmdQueue || 
		!swapChain   || backBufferCount == 0)
		return false;

	m_d3d12Device = d3d12Device;

	// 创建D3D11On12设备
	 
	//命令队列 (转成基类指针用于创建)
	IUnknown* queues[] = { cmdQueue };
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1 };
	D3D_FEATURE_LEVEL featureLevel;//特效级别

	// D3D11On12设备创建
	HRESULT hr = D3D11On12CreateDevice(
		m_d3d12Device,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		featureLevels,
		_countof(featureLevels),
		queues,
		_countof(queues),
		0,
		&m_d3d11Device, // 输出虚拟D11设备
		&m_d3d11Context,// 输出虚拟D11上下文
		&featureLevel);

	if (FAILED(hr))return false;
	//查询是否有D3D11On12设备,有的话给m_d3d11On12Device
	hr = m_d3d11Device.As(&m_d3d11On12Device);
	if (FAILED(hr)) return false;

	//创建D2D工厂

	D2D1_FACTORY_OPTIONS options = {};
#if defined(_DEBUG)
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;//开启调试
#endif
	hr = D2D1CreateFactory(
		D2D1_FACTORY_TYPE_SINGLE_THREADED,//单线程工厂
		__uuidof(ID2D1Factory1),
		&options,
		(void**)m_d2dFactory.GetAddressOf());
	if (FAILED(hr)) return false;

	//创建dxgi设备
	Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
	hr = m_d3d11Device.As(&dxgiDevice);
	if (FAILED(hr)) return false;

	//创建D2D设备(通过dxgi创建)
	hr = m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice);
	if (FAILED(hr)) return false;

	//创建D2D上下文
	hr = m_d2dDevice->CreateDeviceContext(
		D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
		&m_d2dContext);
	if (FAILED(hr)) return false;

	//统一D2D与DX12的坐标单位
	//使用屏幕像素坐标作为单位 96DPI → 1DIP = 1 物理像素
	m_d2dContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
	m_d2dContext->SetDpi(96.0f, 96.0f); //96 DPI

	//创建DirectWrite 工厂
	hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
	if (FAILED(hr)) return false;

	//添加初始字体
	AddTextFormat(ETextType::DEBUG, L"Microsoft YaHei", 18.0f,
		DWRITE_TEXT_ALIGNMENT_LEADING);
	AddTextFormat(ETextType::NPC_NAME, L"Microsoft YaHei", 16.0f,
		DWRITE_TEXT_ALIGNMENT_CENTER);

	if (!CreateSwapChainResources(swapChain, backBufferCount)) return false;

	m_isInitialized = true;
	return true;
}

void D2DTextRenderer::ReleaseResource()
{
	if (m_d2dRenderTargets.empty() && m_wrappedBackBuffers.empty())
		return;   // 已经释放过了
	//释放交换链资源
	ReleaseSwapChainResources();
}

void D2DTextRenderer::ReBuildResource(IDXGISwapChain* swapChain, UINT bufferCount)
{
	//重建交换链资源
	CreateSwapChainResources(swapChain,bufferCount);
}

void D2DTextRenderer::Begin(UINT backBufferIndex)
{
	if (m_isDrawing) return;
	if (!m_d2dContext || !m_d3d11On12Device || !m_d3d11Context) return;
	if (backBufferIndex >= m_d2dRenderTargets.size()) return;
	if (!m_d2dRenderTargets[backBufferIndex]) return;

	m_currentBackBufferIndex = backBufferIndex;

	//拿到后台缓冲所有权
	ID3D11Resource* resources[] = {
		m_wrappedBackBuffers[backBufferIndex].Get()
	};
	m_d3d11On12Device->AcquireWrappedResources(resources, 1);

	// 设置d2d绘制目标
	m_d2dContext->SetTarget(m_d2dRenderTargets[backBufferIndex].Get());

	// d2d绘制
	m_d2dContext->BeginDraw();


	// 创建并缓存笔刷
	if (!m_brush)
		m_d2dContext->CreateSolidColorBrush(
			D2D1::ColorF(1, 1, 1, 1), &m_brush);//实心
	if (!m_outlineBrush)
		m_d2dContext->CreateSolidColorBrush(
			D2D1::ColorF(0, 0, 0, 1), &m_outlineBrush);//描边

	m_isDrawing = true;//标记正在绘制


}
void D2DTextRenderer::End()
{
	if (!m_isDrawing) return;
	HRESULT hr = m_d2dContext->EndDraw();//结束绘制

	//归还缓冲区所有权
	ID3D11Resource* resources[] = {
	   m_wrappedBackBuffers[m_currentBackBufferIndex].Get()
	};
	m_d3d11On12Device->ReleaseWrappedResources(resources, 1);

	//借由D11命令上下文将命提交给D12的命令队列
	m_d3d11Context->Flush();
	m_isDrawing = false;//绘制完成

	// d2d层面渲染目标失效的话,需要释放d2d相关资源,在下一帧重建
	// (窗口缩放)
	if (FAILED(hr))
	{
		if (m_d2dContext)
		{
			m_d2dContext->SetTarget(nullptr);
			m_d2dContext->Flush();
		}
		m_d2dRenderTargets.clear();
		m_brush.Reset();
		m_outlineBrush.Reset();
		
	}

}

void D2DTextRenderer::DrawD2DText(const std::wstring& text, 
								  float startX, float startY, 
								  const D2D1_COLOR_F& color, 
								  ETextType fmt,
								  bool outline)
{

	if (!m_isDrawing || text.empty()) return;

	auto* format = GetFormat(fmt);
	if (!format) return;

	// 超大矩形 = 不换行、不限宽
	D2D1_RECT_F rect = D2D1::RectF(
		startX, startY, 
		startX + 4096.0f, startY + 4096.0f);

	//绘制描边字体:上下左右偏移黑色像素绘制字体描边
	if (outline)
	{
		DrawOutlined(text, rect, format, color);
	}
	else
	{
		m_brush->SetColor(color);
		m_d2dContext->DrawTextW(
			text.c_str(),
			(UINT32)text.size(),
			format,
			rect, 
			m_brush.Get());
	}

}

void D2DTextRenderer::DrawD2DCenter(const std::wstring& text, float cx, float cy, const D2D1_COLOR_F& color, ETextType fmt, bool outline)
{
	if (!m_isDrawing) return;

	auto* format = GetFormat(fmt);
	if (!format) return;

	// 1. 测量文字实际宽高（单行模式）
	DWRITE_TEXT_METRICS metrics;
	Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
	m_dwriteFactory->CreateTextLayout(
		text.c_str(),
		(UINT32)text.size(),
		format,
		FLT_MAX,    // 不限制宽度，强制单行
		FLT_MAX,
		&layout
	);
	layout->GetMetrics(&metrics);

	float textW = metrics.width;
	float textH = metrics.height;

	// 2. 计算居中矩形：矩形中心正好落在 (cx, cy)
	D2D1_RECT_F rect = D2D1::RectF(
		cx - textW * 0.5f,
		cy - textH * 0.5f,
		cx + textW * 0.5f,
		cy + textH * 0.5f
	);

	//绘制描边或纯色
	if (outline)
	{
		DrawOutlined(text, rect, format, color);
	}
	else
	{
		m_brush->SetColor(color);
		m_d2dContext->DrawTextW(
			text.c_str(), 
			(UINT32)text.size(),
			format,
			rect,
			m_brush.Get());
	}
}

void D2DTextRenderer::AddTextFormat( ETextType id, 
									const wchar_t* fontName, 
									float fontSize, 
									DWRITE_TEXT_ALIGNMENT align)
{
	if (!m_dwriteFactory) return;

	// 创建文字样式
	Microsoft::WRL::ComPtr<IDWriteTextFormat> fmt;
	HRESULT hr = m_dwriteFactory->CreateTextFormat(
		fontName,                  //字体名
		nullptr,                   //系统默认字体库
		DWRITE_FONT_WEIGHT_NORMAL, //字重
		DWRITE_FONT_STYLE_NORMAL,  //样式
		DWRITE_FONT_STRETCH_NORMAL,//拉伸
		fontSize,                  //字大小(单位DIP)
		L"zh-cn",                  //中文
		fmt.GetAddressOf());       //输出
	if (FAILED(hr)) return;

	fmt->SetTextAlignment(align);                               // 水平对齐
	fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);// 段落垂直对齐
	fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);         // 关闭自动换行

	//添加到列表
	m_formats[(int)id] = fmt;

}

bool D2DTextRenderer::CreateSwapChainResources(IDXGISwapChain* swapChain, UINT bufferCount)
{
	if (!m_d3d11On12Device || !m_d2dContext ||
		!swapChain || bufferCount == 0) return false;

	//重置包装缓冲区
	m_wrappedBackBuffers.resize(bufferCount);
	//重置D2D渲染目标
	m_d2dRenderTargets.resize(bufferCount);

	for (UINT i = 0; i < bufferCount; ++i)
	{
		//拿到外部后台缓冲区资源
		Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
		HRESULT hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));
		if (FAILED(hr)) return false;

		//设置D11以什么格式包装资源
		D3D11_RESOURCE_FLAGS flags = { D3D11_BIND_RENDER_TARGET };

		//创建包装资源
		hr = m_d3d11On12Device->CreateWrappedResource(
			backBuffer.Get(),
			&flags,
			D3D12_RESOURCE_STATE_RENDER_TARGET,//D2D使用时状态(渲染目标)
			D3D12_RESOURCE_STATE_PRESENT,//归还时状态(呈现)
			IID_PPV_ARGS(m_wrappedBackBuffers[i].GetAddressOf()));
		if (FAILED(hr)) return false;

		//dxgi表面(D2D只能使用这一格式创建bitMap,D11资源要先拿到它)
		Microsoft::WRL::ComPtr<IDXGISurface> surface;
		hr = m_wrappedBackBuffers[i].As(&surface);
		if (FAILED(hr)) return false;

		// 1. 获取 IDXGISurface 的实际描述
		DXGI_SURFACE_DESC surfaceDesc = {};
		surface.Get()->GetDesc(&surfaceDesc);

		//bitMap绘图目标对象的描述
		D2D1_BITMAP_PROPERTIES1 bitMapProp = {};
		bitMapProp.pixelFormat.format =     surfaceDesc.Format;
		bitMapProp.pixelFormat.alphaMode =  D2D1_ALPHA_MODE_IGNORE;
		bitMapProp.bitmapOptions =			D2D1_BITMAP_OPTIONS_TARGET |
											D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
		bitMapProp.dpiX = 96.0f;
		bitMapProp.dpiY = 96.0f;

		//创建bitMap绘图目标对象(只能通过显存表面创建)
		hr = m_d2dContext->CreateBitmapFromDxgiSurface(
			surface.Get(),
			&bitMapProp,
			&m_d2dRenderTargets[i]);
		if (FAILED(hr)) return false;
	}
	
	return true;
}

void D2DTextRenderer::ReleaseSwapChainResources()
{
	if (m_d2dContext)
	{
		m_d2dContext->SetTarget(nullptr);
		m_d2dContext->Flush();
	}

	m_d2dRenderTargets.clear();
	m_brush.Reset();
	m_outlineBrush.Reset();

	if (m_d3d11Context)
	{
		m_d3d11Context->OMSetRenderTargets(0, nullptr, nullptr);
		m_d3d11Context->Flush();
		m_d3d11Context->ClearState();
	}
	m_wrappedBackBuffers.clear(); // 再释放D3D11包装
	m_currentBackBufferIndex = 0;
	m_isDrawing = false;
}

IDWriteTextFormat* D2DTextRenderer::GetFormat(ETextType id)
{
	auto it = m_formats.find(static_cast<int>(id));
	return it == m_formats.end() ? nullptr : it->second.Get();
}

void D2DTextRenderer::DrawOutlined(const std::wstring& text, const D2D1_RECT_F& rect, IDWriteTextFormat* format, const D2D1_COLOR_F& color)
{

	//绘制描边字体:上下左右偏移黑色像素绘制字体描边
	 
	
	// 4 个方向偏移画黑字
	m_outlineBrush->SetColor(D2D1::ColorF(0, 0, 0, color.a));

	const float d = 1.0f;
	const D2D1_RECT_F offsets[4] = {
		D2D1::RectF(rect.left - d, rect.top,     rect.right - d, rect.bottom    ),//左
		D2D1::RectF(rect.left + d, rect.top,     rect.right + d, rect.bottom    ),//右
		D2D1::RectF(rect.left,     rect.top - d, rect.right,     rect.bottom - d),//下
		D2D1::RectF(rect.left,     rect.top + d, rect.right,     rect.bottom + d),//上
	};

	//4个方向绘制
	for (auto& r : offsets)
	{
		m_d2dContext->DrawTextW(
			text.c_str(),
			(UINT32)text.size(),
			format,
			r, 
			m_outlineBrush.Get());
	}

	// 正式颜色
	m_brush->SetColor(color);
	m_d2dContext->DrawTextW(
		text.c_str(), 
		(UINT32)text.size(),
		format, 
		rect, 
		m_brush.Get());

}

