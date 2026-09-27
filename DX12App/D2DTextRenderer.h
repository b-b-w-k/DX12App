#pragma once


#include <d3d12.h>
#include <dxgi.h>

#include <d3d11on12.h>//d12->d11转接

#include <d2d1_1.h>//2d文本渲染
#include <dwrite.h>

#include <wrl/client.h>//Com指针

#include <string>
#include <vector>
#include <unordered_map>


enum class ETextType
{
    DEBUG,   // 调试信息（左对齐）
    NPC_NAME,// NPC 名字（居中对齐）
    UI_INFO, // UI信息
};

//封装Text绘制逻辑
//使用D3D11On12 互操作层,通过D11调用D2D/DWrite 绘制文本内容

class D2DTextRenderer
{
public:

    D2DTextRenderer() = default;
    ~D2DTextRenderer();
    D2DTextRenderer(const D2DTextRenderer&) = delete;
    D2DTextRenderer& operator=(const D2DTextRenderer&) = delete;

    //要等D12初始化完成后传入参数
    bool InitD2DRender( ID3D12Device* d3d12Device,   //传入d12设备
                        ID3D12CommandQueue* cmdQueue,//命令队列
                        IDXGISwapChain* swapChain,   //交换链
                        UINT backBufferCount);       //后台缓冲区数量
    
    //释放包装资源(Resize交换链之前)
    void ReleaseResource();
    //重建包装资源(Resize交换链之后)
    void ReBuildResource(IDXGISwapChain* swapChain,
                         UINT bufferCount);

    //开始绘制D2D
    void Begin(UINT backBufferIndex);

    //结束绘制
    void End();

    //绘制接口(默认左上角开始绘制,调试文本信息)
    void DrawD2DText(const std::wstring& text,         //文本
                     float startX, float startY,       //开始位置
                     const D2D1_COLOR_F& color,        //颜色
                     ETextType fmt = ETextType::DEBUG, //文本类型
                     bool outline = false);            //描边
    //绘制接口(默认底部中心,NPC名字)
    void DrawD2DCenter( const std::wstring& text,
                        float cx, float cy,
                        const D2D1_COLOR_F& color,
                        ETextType fmt = ETextType::NPC_NAME,
                        bool outline = true);

    
    //添加文本格式
    void AddTextFormat( ETextType type,           //文本类型
                        const wchar_t* fontName,  //字体名
                        float fontSize,           //字体大小
                        DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING);//前端对齐

    //初始化释放完成
    bool IsInit() const { return m_isInitialized; }

private:

    bool CreateSwapChainResources(IDXGISwapChain* swapChain, UINT bufferCount);
    void ReleaseSwapChainResources();

    IDWriteTextFormat* GetFormat(ETextType id);  //获取格式

    //描边绘制
    void DrawOutlined(const std::wstring& text,
                      const D2D1_RECT_F& rect,
                      IDWriteTextFormat* format,
                      const D2D1_COLOR_F& color);

private:

    ID3D12Device* m_d3d12Device    = nullptr;   // d12设备引用
    ID3D12CommandQueue* m_cmdQueue = nullptr;   // d12命令队列引用

    //===== D3D11On12 =====
    Microsoft::WRL::ComPtr<ID3D11Device>        m_d3d11Device;     // d3d11设备
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11Context;    // D11立即上下文(类似D12命令列表)用于提交命令
    Microsoft::WRL::ComPtr<ID3D11On12Device>    m_d3d11On12Device; // 转接器(将D12资源包装成D11资源)
                                                                      
    //===== D2D ===========                                           
    Microsoft::WRL::ComPtr<ID2D1Factory1>        m_d2dFactory;     // d2d工厂
    Microsoft::WRL::ComPtr<ID2D1Device>          m_d2dDevice;      // d2d设备
    Microsoft::WRL::ComPtr<ID2D1DeviceContext>   m_d2dContext;     // D2D立即上下文
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brush;          // 纯色笔刷
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_outlineBrush;   // 线条笔刷

    //===== DWrite ========
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;        //文本工厂,创建文本和格式
    std::unordered_map<int, Microsoft::WRL::ComPtr<IDWriteTextFormat>> m_formats;//存储文字格式

    //===== 交换链资源（每个后台缓冲一份包装） ========
    std::vector<Microsoft::WRL::ComPtr<ID3D11Resource>> m_wrappedBackBuffers;//包装成D11的后台缓冲区资源数组
    std::vector<Microsoft::WRL::ComPtr<ID2D1Bitmap1>>   m_d2dRenderTargets;  //用于D2D绘制的渲染目标(D11资源转换为D2DBitmap)

    UINT m_currentBackBufferIndex = 0; //当前后台缓冲区的索引
    bool m_isDrawing =     false;      //是否正在绘制
    bool m_isInitialized = false;      //是否完成初始化

};

