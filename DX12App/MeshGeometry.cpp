#include "MeshGeometry.h"

D3D12_VERTEX_BUFFER_VIEW MeshGeometry::GetVertexBufferView() const
{
	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = vertexBufferGPU->GetGPUVirtualAddress();
	vbv.StrideInBytes = vertexByteStride;
	vbv.SizeInBytes = vertexByteSize;
	return vbv;
}

D3D12_INDEX_BUFFER_VIEW MeshGeometry::GetIndexBufferView() const
{
	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = indexBufferGPU->GetGPUVirtualAddress();
	ibv.Format = indexFormat;
	ibv.SizeInBytes = indexByteSize;
	return ibv;
}

void MeshGeometry::ReleaseUploaders()
{
	vertexBufferUPload = nullptr;
	indexBufferUPload = nullptr;
}