#pragma once
#include"D3DTools.h"
#include "d3dx12.h"


//载入缓冲区
template <class T>
class UpLoadBuffer
{
public:
	UpLoadBuffer(ID3D12Device* device, UINT elementCount, bool isCBuffer);
	UpLoadBuffer& operator=(const UpLoadBuffer&) = delete;
	UpLoadBuffer(const UpLoadBuffer&) = delete;
	~UpLoadBuffer();

	ID3D12Resource* Resource()const;

	void CopyData(int elementIndex, const T& data);//复制一个数据
	void CopyData(const T* data, UINT count, UINT elementIndex = 0);//复制一批数据

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_upLoadBuffer;
	BYTE* m_mapData = nullptr; //CPU读取地址

	UINT m_elementSize = 0;
	bool m_isCBuffer = false;
};

template<class T>
inline UpLoadBuffer<T>::UpLoadBuffer(ID3D12Device* device, UINT elementCount, bool isCBuffer)
{
	m_elementSize = sizeof(T);

	m_isCBuffer = isCBuffer;

	if (isCBuffer) m_elementSize = D3DTools::BufferByteSize(m_elementSize);//256字节对齐

	//创建到上传堆
	auto upHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto buffer = CD3DX12_RESOURCE_DESC::Buffer(m_elementSize * elementCount);
	ThrowIfFailed(device->CreateCommittedResource(
		&upHeap,
		D3D12_HEAP_FLAG_NONE,
		&buffer,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_upLoadBuffer)));

	//堆数据映射到内存
	ThrowIfFailed(m_upLoadBuffer->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&m_mapData)));
}

template<class T>
inline UpLoadBuffer<T>::~UpLoadBuffer()
{
	if (m_upLoadBuffer)m_upLoadBuffer->Unmap(0, nullptr);
	m_mapData = nullptr;
}

template<class T>
inline ID3D12Resource* UpLoadBuffer<T>::Resource() const
{
	return m_upLoadBuffer.Get();
}

template<class T>
inline void UpLoadBuffer<T>::CopyData(int elementIndex, const T& data)
{
	memcpy(&m_mapData[m_elementSize * elementIndex], &data, sizeof(T));
}

template<class T>
inline void UpLoadBuffer<T>::CopyData(const T* data, UINT count, UINT elementIndex)
{
	if (m_elementSize == sizeof(T))
	{
		// 非CB内存连续直接整块拷贝
		memcpy(&m_mapData[m_elementSize * elementIndex],
			data,
			sizeof(T) * count);
	}
	else
	{
		// CB每个元素有对齐填充必须逐个拷贝
		for (UINT i = 0; i < count; ++i)
		{
			memcpy(&m_mapData[m_elementSize * (elementIndex + i)],
				&data[i],
				sizeof(T));
		}
	}
}
