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

		std::vector<float> GetNormalizePixels() const
		{
			std::vector<float> out;
			if (pixels.empty() || bytesPerPixel == 0) return out;
			const size_t count = (size_t)pixelWidth * pixelHeight;
			out.resize(count, 0.0f);

			const uint8_t* p = pixels.data();

			//判断格式
			if (bytesPerPixel == 1) // R8_UNORM
			{
				for (size_t i = 0; i < count; ++i) 
					out[i] = p[i] / 255.0f;
			}
			else if (bytesPerPixel == 2) // R16_UNORM（DDS 小端）
			{
				for (size_t i = 0; i < count; ++i)
				{
					//将后八位(高位)*256+前8位(低位)得到16位整型表示
					const uint16_t v = (uint16_t)(p[i * 2] + (p[i * 2 + 1] * 256));
					//归到0-1之间
					out[i] = v / 65535.0f;
				}
			}
			else if (bytesPerPixel == 4) // R8G8B8A8/B8G8R8A8：取第 0 字节
			{
				for (size_t i = 0; i < count; ++i) 
					out[i] = p[i * 4 + 0] / 255.0f;
			}

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

	//将纹理上传到GPU ,区别:纹理数据来源为从一维容器中获取的纹素
	HRESULT CreateTextureFromPixels(_In_  ID3D12Device* device,
									_In_  ID3D12GraphicsCommandList* cmdList,
									_In_  const uint8_t* pixels,
									_In_  UINT width,
									_In_  UINT height,
									_In_  DXGI_FORMAT format,
									_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
									_Out_ Microsoft::WRL::ComPtr<ID3D12Resource>& textureUploadHeap);
private:
	//将Subresource数组中的数据拷贝进DDSTextureResult::pixels中
	void CopyDataFromSubresource(
		_In_  const D3D12_SUBRESOURCE_DATA& src,
		_In_  UINT width,
		_In_  UINT height,
		_In_  DXGI_FORMAT format,
		_Out_ DDSTextureResult& outResult);

};

