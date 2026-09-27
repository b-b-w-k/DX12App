#pragma once
#include <d3d12.h>
#include <wrl.h>

#include <vector>
#include <cstdint>

//DDS纹理导入包装类
//功能:纹理导入,上传GPU资源
//可选EX:CPU端资源备份
//详解:将DDS文件借由DDSTextureLoader12解析获得资源
//借助上传堆写入GPU资源

//前向声明
namespace DirectX
{
	enum DDS_ALPHA_MODE : uint32_t;
}


class DDSTextureLoader
{
public:
	// 保存DDS文件的信息用于CPU调度
	struct DDSTextureResult
	{
		std::vector<uint8_t> pixels;                   // 纹素(mip0)
		UINT pixelWidth    = 0;						   // 图片宽
		UINT pixelHeight   = 0;						   // 图片高
		UINT bytesPerPixel = 0;                        // 字节数
		DXGI_FORMAT pixelFormat = DXGI_FORMAT_UNKNOWN; // 图片格式

		// 是否有纹素
		bool isPixels() { return !pixels.empty(); }
		//获取纹素(仅限R8,单通道8位)
		uint8_t GetPixel(UINT x, UINT y) const
		{
			if (x >= pixelWidth || y >= pixelHeight) return 0;
			return pixels[y * pixelWidth + x];
		}
		//获取归一化纹素(仅限R8,单通道8位)
		float GetNormalizePixel(UINT x, UINT y) const
		{
			return GetPixel(x, y) / 255.0f;
		}
		std::vector<float> GetNormalizePixels() const
		{
			if (pixels.empty() || bytesPerPixel == 0)
				return {};
			const size_t count = (size_t)pixelWidth * pixelHeight;
			std::vector<float> out(count);

			const uint8_t* p = pixels.data();
			for (size_t i = 0; i < count; ++i)
				out[i] = p[i] / 255.0f;
			return out;

		}
		static UINT GetBytesPerPixel(DXGI_FORMAT fmt);
	};

	//将DDS纹理上传到GPU,返回texture 和 uploadHeap
	HRESULT CreateDDSTextureFromFile12( _In_	  ID3D12Device* device,
										_In_	  ID3D12GraphicsCommandList* cmdList,
										_In_z_	  const wchar_t* szFileName,
										_Out_	  Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
										_Out_	  Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap,
										_In_	  size_t maxsize = 0,
										_Out_opt_ DirectX::DDS_ALPHA_MODE* alphaMode = nullptr);

	//将DDS纹理上传到GPU,返回texture 和 uploadHeap,可选:返回纹素信息
	HRESULT CreateDDSTextureFromFile12Ex(_In_	   ID3D12Device* device,
										 _In_	   ID3D12GraphicsCommandList* cmdList,
										 _In_z_	   const wchar_t* szFileName,
										 _Out_	   Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
										 _Out_	   Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap,
										 _Out_	   DDSTextureResult* pTextureResult = nullptr,
										 _In_	   size_t maxsize = 0,
										 _Out_opt_ DirectX::DDS_ALPHA_MODE * alphaMode = nullptr);


private:
	//将Subresource数组中的数据拷贝进DDSTextureResult::pixels中
	void CopyDataFromSubresource(
		_In_  const D3D12_SUBRESOURCE_DATA& src,
		_In_  UINT width,
		_In_  UINT height,
		_In_  DXGI_FORMAT format,
		_Out_ DDSTextureResult& outResult);

};

