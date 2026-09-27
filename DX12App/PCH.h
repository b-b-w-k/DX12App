#pragma once

// PCH.h
// 预编译头文件
// +只允许cpp文件包含+

// 包含常用头文件,避免cpp文件重复解析

// 标准库
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <array>
#include <queue>
#include <deque>
#include <functional>
#include <cstdint>
#include <cassert>
#include <algorithm>

// 系统头
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

// DirectX
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcommon.h>
#include <d3dcompiler.h>
#include <wrl.h>

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <DirectXColors.h>

#include <d3dx12.h>


