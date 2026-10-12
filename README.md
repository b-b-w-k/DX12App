# DX12App

一个用 C++ / DirectX 12 从零手写的 3D 渲染器：描述符堆、根签名、PSO、多帧资源、硬件曲面细分、蒙皮动画、碰撞与射线检测全部自研，不依赖任何现成引擎（UE / Unity）。

> 学习项目，目的是把现代图形管线的每个环节亲手实现一遍。

## 特性

| 模块 | 说明 |
| --- | --- |
| 渲染核心 | 描述符堆 / 根签名 / PSO / 多帧资源并行（FrameResource）/ 命令列表录制与提交 |
| 相机 | 第一人称,第三人称与自由相机控制器，视锥体构建与剔除 |
| 场景与组件 | 节点树 + 组件式结构（渲染 / 蒙皮 / 碰撞 / 控制），广度优先更新世界变换 |
| 材质与光照 | 基础色 + 法线贴图，多材质按权重分层混合，方向光 / 点光 / 聚光灯，环境光 |
| MSAA | 4x MSAA 及 MSAA 深度模板，随窗口尺寸变化重建 |
| 天空盒 | 立方体贴图，独立 CUBE SRV 与 PSO |
| 反射 | 水面平面反射 RTT、环境立方反射 RTT、水面波动着色器 |
| 地形 | 高度图 CPU 采样 + 高度/法线烘焙到 RGBA16 纹理 + 四叉树 LOD + 硬件曲面细分（HS/DS）+ 实例化渲染 patch |
| 模型与动画 | assimp 导入 FBX，骨骼层级 + 动画片段采样 + 蒙皮 |
| 碰撞与射线 | AABB / OBB / 三角形 / 地形高度图；射线拾取、移动检测、地面高度、摄像机防穿墙 |
| 调试可视化 | 包围盒 / 视锥体线框渲染，Direct2D + DirectWrite 屏幕文字 |

## 环境要求

- Windows 10 / 11（x64）
- Visual Studio 2022（v143 工具集 + Windows SDK）
- 支持 DirectX 12 的显卡（未做 D3D11 回退）
- **assimp 运行时 DLL**：工程链接的是 `outside/lib/assimpd.lib`（导入库），运行需要把 `libassimp-5d.dll` 放到可执行文件同目录（`bin\`）。本仓库不含该 DLL，请自行编译 assimp，或从 assimp 官方 release 获取对应版本。

## 构建与运行

1. 用 VS2022 打开 `DX12App.sln`
2. 选择配置 **Debug | x64**（目前只有该配置设置了 `OutDir=../bin`、`IntDir=../temp`）
3. 按 F5 运行

> **工作目录很关键**：纹理、模型、着色器都按 `../textures/`、`../models/`、`../shaders/` 的相对路径查找（HLSL 在运行时编译）。VS 调试时工作目录默认是 `DX12App\DX12App\`，正好正确；如果要手动运行 `bin\DX12App.exe`，必须把工作目录设成 `DX12App\DX12App\`。

## 需要自备的资源

本仓库不包含贴图与模型（体积大、授权来源不统一）。请自行准备下列文件，放到仓库根目录的 `textures/` 与 `models/` 目录下 —— 目录名和**扩展名大小写必须完全一致**（Windows 不区分，Linux / CI 区分）。

`models/`

| 文件 | 说明 |
| --- | --- |
| `AK-H.fbx` | 静态网格模型 |
| `Start Walking.fbx` | 带动画片段的模型 |

`textures/`

| 文件 | 说明 |
| --- | --- |
| `terrain_hei_U16.dds` | 地形高度图，必须是 `DXGI_FORMAT_R16_UNORM`，1080×1080 |
| `skyBox.dds` | 天空盒立方体贴图 |
| `brick_diff.dds` / `brick_nor.dds` | 砖墙材质：基础色 / 法线 |
| `grass_diff.dds` / `grass_nor.dds` | 草地材质 |
| `grass1_diff.dds` / `grass1_nor.dds` | 草地材质（第二种） |
| `rock_diff.dds` / `rock_nor.dds` | 岩石材质 |
| `shashi_diff.dds` / `shashi_nor.dds` | 砂石材质 |
| `shazi_diff.dds` / `shazi_nor.dds` | 沙地材质 |
| `water_high_nor.dds` / `water_low_nor.dds` | 水面法线（高 / 低） |

缺少任一文件都会加载失败并弹出错误提示。

## 操作说明

| 按键 | 功能 |
| --- | --- |
| F1 | 锁定 / 解锁鼠标 |
| F2 | 锁定视锥体 |
| F3 | 视锥体可视化 |
| F4 | AABB 包围盒可视化 |
| F5 | OBB 包围盒可视化 |
| F6 | 调试信息面板 |

## 目录结构

```
DX12App/
├─ DX12App.sln
├─ DX12App/                  # 全部源码
│  ├─ GameApp.*                渲染框架基类（设备 / 交换链 / 描述符堆 / PSO / 帧资源）
│  ├─ BoxGameApp.*             主程序：场景搭建、输入绑定、调试开关
│  ├─ Scene.*                  场景与节点、纹理与模型注册、包围盒更新
│  ├─ Node.* / Component.h     节点树与组件基类
│  ├─ RenderComponent.* / SkeletonComponent.* / CollisionComponent.* / ControlComponent.*
│  ├─ Camera.* / ICameraController.*
│  ├─ TerrainHeightField.*     高度图采样、高度与法线烘焙
│  ├─ QuadTree.*               地形四叉树 LOD 与 patch 管理
│  ├─ RayCaster.*              射线拾取、移动检测、地面高度
│  ├─ ReflectionRTT.*          反射渲染目标
│  ├─ GeoGenerator.*           程序化几何 + assimp FBX 导入
│  ├─ Skeleton.h / AnimationClip.*
│  ├─ D2DTextRenderer.* / DebugLineRenderer.* / Frustum.h
│  ├─ D3DTools.* / UpLoadBuffer.h / FrameResource.* / GameTimer.*
│  └─ DDSTextureLoader12.*     DDS 纹理加载（Microsoft，MIT）
├─ shaders/                  # HLSL，运行时编译
│  ├─ common.hlsl / light.hlsl / standard.hlsl / color.hlsl
│  └─ terrain.hlsl / mirror.hlsl / wave.hlsl / skyBox.hlsl
├─ outside/                  # 第三方依赖源码与导入库（已内置）
├─ textures/                 # 贴图（需自备，不纳入版本管理）
├─ models/                   # FBX 模型（需自备，不纳入版本管理）
└─ bin/ temp/                # 输出与中间目录（不纳入版本管理）
```

## 第三方依赖

| 依赖 | 位置 | 许可证 |
| --- | --- | --- |
| assimp | `outside/include/assimp`、`outside/lib/assimpd.lib` | BSD-3-Clause |
| d3dx12.h | `outside/include/d3dx12.h` | MIT（Microsoft） |
| DDSTextureLoader12 | `DX12App/DDSTextureLoader12.*` | MIT（Microsoft） |

各依赖的完整许可文本见 [NOTICE.md](NOTICE.md)。

## 素材说明

`textures/` 与 `models/` 下的素材不随本仓库分发，也不在本仓库的代码许可证覆盖范围内。请自行确认所用素材的来源与授权。

## 已知限制

- 仅支持 Windows / DirectX 12，无 D3D11 或 Vulkan 回退
- 阴影（Shadow Map）尚未实现(未来补充)
- 目前只有 `Debug|x64` 配置经过验证
- 地形与部分几何的 LOD 策略仍在调整中

## License

本项目代码以 MIT 许可证发布，见 [LICENSE](LICENSE)。
第三方依赖的许可情况见 [NOTICE.md](NOTICE.md)。
