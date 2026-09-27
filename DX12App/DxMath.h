#pragma once
#include <DirectXMath.h>

namespace DxMath
{
	// 单位矩阵
	inline DirectX::XMFLOAT4X4 Identity4x4() noexcept
	{
		return DirectX::XMFLOAT4X4(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);
	}
}