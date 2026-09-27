#pragma once

#include <dxgi1_4.h>
#include <d3d12.h>
#include <wrl.h>
#include "DxException.h"
#include "DxMath.h"

// Dx工具函数
class D3DTools
{
public:

	template<typename T>
	static T Clamp(const T& x, const T& low, const T& high)
	{
		return x > low ? (x < high ? x : high) : low;
	}

	//返回256字节对齐的字节数
	static UINT BufferByteSize(UINT byteSize);

	//编译着色器(路径,传入宏,入口函数名,着色器模型)
	static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring& fileName,
		const D3D_SHADER_MACRO* shaderDefines,
		const std::string& entrypoint,
		const std::string& target);

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

};
