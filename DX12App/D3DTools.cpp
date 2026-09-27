#include "PCH.h"
#include "D3DTools.h"
#include "d3dx12.h"


DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber)
	:
	ErrorCode(hr),
	FunctionName(functionName),
	Filename(filename),
	LineNumber(lineNumber)
{
}

std::wstring DxException::ToString() const
{
	_com_error err(ErrorCode);
	std::wstring msg = err.ErrorMessage();

	return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
}

UINT D3DTools::BufferByteSize(UINT byteSize)
{
	//256字节向上对齐(byteSize& ~255向下对齐)
	return (byteSize + 255) & ~255;
}

Microsoft::WRL::ComPtr<ID3DBlob> D3DTools::CompileShader(
	const std::wstring& fileName,
	const D3D_SHADER_MACRO* shaderDefines,
	const std::string& entrypoint,
	const std::string& target)
{
	//hlsl调试选项
	UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = S_OK;

	Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorInfo = nullptr;

	hr = D3DCompileFromFile(
		fileName.c_str(),
		shaderDefines,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(),
		target.c_str(),
		compileFlags,
		0,
		&byteCode,
		&errorInfo);
	//先打印错误信息

	if (errorInfo)
		OutputDebugStringA((char*)errorInfo->GetBufferPointer());
	ThrowIfFailed(hr);

	return byteCode;
}

Microsoft::WRL::ComPtr<ID3D12Resource> D3DTools::CreateDefaultBuffer(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmdList,
	const void* initData,
	UINT64 byteSize,
	Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
	Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;

	//创建默认缓冲区
	auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);//默认堆
	auto buffer = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	ThrowIfFailed(device->CreateCommittedResource(
		&heap,
		D3D12_HEAP_FLAG_NONE,
		&buffer,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

	//创建上传缓冲区(用于数据中转CPU->GPU)
	heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);//上传堆
	ThrowIfFailed(device->CreateCommittedResource(
		&heap,
		D3D12_HEAP_FLAG_NONE,
		&buffer,
		D3D12_RESOURCE_STATE_GENERIC_READ,//只读
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = byteSize;
	subResourceData.SlicePitch = byteSize;

	//资源状态转换(默认->复制数据)
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_COPY_DEST);
	cmdList->ResourceBarrier(1, &barrier);

	//只能传一维数据
	UpdateSubresources<1>(
		cmdList,
		defaultBuffer.Get(),
		uploadBuffer.Get(),
		0, 0, 1,
		&subResourceData);

	//资源状态转换(复制数据->只读)
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_GENERIC_READ);
	cmdList->ResourceBarrier(1, &barrier);

	return defaultBuffer;
}