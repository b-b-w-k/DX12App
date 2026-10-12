#include "PCH.h"
#include "DDSTextureLoader.h"
#include"DDSTextureLoader12.h"
#include "d3dx12.h" 


HRESULT DDSTextureLoader::CreateDDSTextureFromFile12( _In_      ID3D12Device* device, 
                                                      _In_      ID3D12GraphicsCommandList* cmdList, 
                                                      _In_z_    const wchar_t* szFileName, 
                                                      _Out_     Microsoft::WRL::ComPtr<ID3D12Resource>& texture, 
                                                      _Out_     Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap, 
                                                      _In_      size_t maxsize, 
                                                      _Out_opt_ DirectX::DDS_ALPHA_MODE* alphaMode)
{
    //转发给EX版
    return CreateDDSTextureFromFile12Ex(
        device,
        cmdList, 
        szFileName,
        texture,
        textureUploadHeap,
        nullptr,    // 不需要返回像素
        maxsize,
        alphaMode);

    
}

HRESULT DDSTextureLoader::CreateDDSTextureFromFile12Ex( _In_	   ID3D12Device* device, 
                                                        _In_	   ID3D12GraphicsCommandList* cmdList, 
                                                        _In_z_	   const wchar_t* szFileName, 
                                                        _Out_	   Microsoft::WRL::ComPtr<ID3D12Resource>& texture, 
                                                        _Out_	   Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap, 
                                                        _Out_	   DDSTextureResult* pTextureResult, 
                                                        _In_	   size_t maxsize, 
                                                        _Out_opt_  DirectX::DDS_ALPHA_MODE* alphaMode)
{

    //清理输出
    texture.Reset();
    textureUploadHeap.Reset();
    if (alphaMode) *alphaMode = DirectX::DDS_ALPHA_MODE_UNKNOWN;

    // 返回无效
    if (!device || !cmdList || !szFileName)
        return E_INVALIDARG;


    //临时保存数据
    std::unique_ptr<uint8_t[]> ddsData;                 //原始字节文件,需保证在subresources调用期间存活
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;   //子资源数组
    Microsoft::WRL::ComPtr<ID3D12Resource> texResource;

    HRESULT hr = DirectX::LoadDDSTextureFromFile(
        device,
        szFileName,
        texResource.GetAddressOf(),
        ddsData,
        subresources,
        maxsize,
        alphaMode);
    if (FAILED(hr)) return hr;

    if (subresources.empty()) return E_FAIL;

    //保存数据到Result
    if (pTextureResult)
    {
        const D3D12_RESOURCE_DESC texDesc = texResource->GetDesc();
        //将subresources[0](mip0)中的数据copy入Result
        CopyDataFromSubresource(
            subresources[0],
            (UINT)texDesc.Width,
            (UINT)texDesc.Height,
            texDesc.Format,
            *pTextureResult);
    }

    //计算上传缓冲区大小
    const UINT subresourcesSize = static_cast<UINT>(subresources.size());;
    const UINT uploadSize = GetRequiredIntermediateSize(
        texResource.Get(), 0, subresourcesSize);

    //创建上传堆
    auto uploadHeapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

    hr = device->CreateCommittedResource(
        &uploadHeapDesc,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(textureUploadHeap.GetAddressOf()));
    if (FAILED(hr)) return hr;

    //资源状态转换
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texResource.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST); //拷贝目标(写入)
    cmdList->ResourceBarrier(1, &barrier);

    //上传数据
    UpdateSubresources(
        cmdList,
        texResource.Get(),
        textureUploadHeap.Get(),
        0,
        0,
        subresourcesSize,
        subresources.data());

    //资源状态转换
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texResource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE); //着色器资源(读)
    cmdList->ResourceBarrier(1, &barrier);

    //输出
    texture = std::move(texResource);
    return S_OK;

}

HRESULT DDSTextureLoader::CreateTextureFromPixels(ID3D12Device* device,
                                                  ID3D12GraphicsCommandList* cmdList,
                                                  const uint8_t* pixels, 
                                                  UINT width, 
                                                  UINT height, 
                                                  DXGI_FORMAT format, 
                                                  Microsoft::WRL::ComPtr<ID3D12Resource>& texture, 
                                                  Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap)
{
	//清理输出
	texture.Reset();
	textureUploadHeap.Reset();

	if (!device || !cmdList || !pixels) return E_INVALIDARG;
	if (width == 0 || height == 0) return E_INVALIDARG;

    //获取每像素字节数
	const UINT bytesPerPixel = DDSTextureResult::GetBytesPerPixel(format);
	if (bytesPerPixel == 0) return E_INVALIDARG;

    const UINT rowPitch = (UINT)(width * bytesPerPixel + 255) & ~255;

    //将数据按256字节对齐打包
    std::vector<uint8_t> packed;
    packed.resize(rowPitch * height);
    for (UINT y = 0; y < height; ++y)
    {
        memcpy(&packed[(size_t)y * rowPitch],               //拷贝目标(偏移256对齐的行距)
               pixels + (size_t)y * width * bytesPerPixel,  //拷贝源(源行距)
               (size_t)width * bytesPerPixel);              //拷贝大小(行距大小)
    }

    //创建纹理资源和默认堆
	auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
	auto defaultHeapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	HRESULT hr = device->CreateCommittedResource(
		&defaultHeapDesc, D3D12_HEAP_FLAG_NONE, &texDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(texture.GetAddressOf()));
	if (FAILED(hr)) return hr;

    //上传堆
	const UINT uploadSize = (UINT)GetRequiredIntermediateSize(texture.Get(), 0, 1);

	auto uploadHeapDesc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

	hr = device->CreateCommittedResource(
		&uploadHeapDesc, D3D12_HEAP_FLAG_NONE, &uploadBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(textureUploadHeap.GetAddressOf()));
	if (FAILED(hr)) return hr;

    //子资源描述
	D3D12_SUBRESOURCE_DATA subresource = {};
	subresource.pData = packed.data();
	subresource.RowPitch = rowPitch;
	subresource.SlicePitch = (UINT)((size_t)rowPitch * height);

    //拷贝资源
	UpdateSubresources(cmdList, texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &subresource);

	//状态转换:拷贝目标->像素着色器资源(读)
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		texture.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	cmdList->ResourceBarrier(1, &barrier);

	return S_OK;
}

void DDSTextureLoader::CopyDataFromSubresource( const D3D12_SUBRESOURCE_DATA& src,
                                                UINT width, 
                                                UINT height, 
                                                DXGI_FORMAT format, 
                                                DDSTextureResult& outResult)
{
    //清理Result数据
    outResult.pixels.clear();
    outResult.pixelWidth = 0;
    outResult.pixelHeight = 0;
    outResult.bytesPerPixel = 0;
    outResult.pixelFormat = DXGI_FORMAT_UNKNOWN;

    if (!src.pData || width == 0 || height == 0)
        return;

    //获取对应压缩格式的字节数
    outResult.bytesPerPixel=DDSTextureResult::GetBytesPerPixel(format);
    if (outResult.bytesPerPixel == 0) return;

    //填充Result
    const UINT widthBytes = outResult.bytesPerPixel * width;
    outResult.pixels.resize((size_t)widthBytes * height);
    outResult.pixelWidth = width;
    outResult.pixelHeight = height;
    outResult.pixelFormat = format;

    //拷贝数据
    const uint8_t* srcBytes = (const uint8_t*)src.pData; 
    const LONG_PTR rowPitch = src.RowPitch;//每行(有字节对齐)的字节数
    //逐行拷贝(src里的原始字节不一定是紧凑的,可能会有字节对齐)
    for (UINT y = 0; y < height; ++y)
    {
        memcpy(&outResult.pixels[(size_t)widthBytes * y],
            srcBytes + (size_t)y * rowPitch,
            widthBytes);
    }
}

UINT DDSTextureLoader::DDSTextureResult::GetBytesPerPixel(DXGI_FORMAT fmt)
{
    switch (fmt)
    {
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_A8_UNORM:               return 1;
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_FLOAT:              return 2;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R32_FLOAT:              return 4;
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R32G32_FLOAT:           return 8;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:     return 16;
    default:                                 return 0;   // 未知
    }
}
