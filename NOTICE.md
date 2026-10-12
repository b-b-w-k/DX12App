# 第三方组件声明

本仓库再分发了以下第三方组件，各自遵循其原始许可证。本仓库自身代码的许可见 [LICENSE](LICENSE)。

---

## 1. assimp（Open Asset Import Library）

- 源码形式：`outside/include/assimp/`（97 个头文件）
- 二进制形式：`outside/lib/assimpd.lib`（导入库）
- 许可证：**BSD-3-Clause**

```
Copyright (c) 2006-2024, assimp team

All rights reserved.

Redistribution and use of this software in source and binary forms,
with or without modification, are permitted provided that the following
conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the assimp team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the assimp team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

> 注：运行本程序还需要 assimp 的运行时动态库 `libassimp-5d.dll`，本仓库不包含该 DLL，请自行编译或从 assimp 官方 release 获取。

---

## 2. d3dx12.h

- 位置：`outside/include/d3dx12.h`
- 许可证：MIT（Microsoft）

该文件取自微软 DirectX Graphics Samples。仓库内这份文件的头部只保留了版权行、未带 MIT 声明文本；如需严格合规，可替换为官方仓库中带 MIT 头部的最新版本。

```
Copyright (c) Microsoft Corporation.
Licensed under the MIT License.
```

---

## 3. DDSTextureLoader12

- 位置：`DX12App/DDSTextureLoader12.h`、`DX12App/DDSTextureLoader12.cpp`
- 许可证：MIT（Microsoft）

```
Copyright (c) Microsoft Corporation.
Licensed under the MIT License.
```

---

## MIT License 文本（适用于上述第 2、3 项）

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
