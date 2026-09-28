#pragma once

//渲染层级(区分不同PSO的绘制对象)
enum class RenderLayer
    :int
{
    Opaque      = 0,   // 不透明渲染
    Wireframe   = 1,   // 线框渲染
    SkyBox      = 2,   // 天空盒渲染
    Debug       = 4,   // 调试层渲染
    Transparent = 5,   // 透明渲染
    AlphaTest   = 6,   // 透明测试渲染
    Mirror      = 7,   // 镜面渲染
	Wave		= 8,   // 波浪渲染
	Terrain	    = 9,   // 地形渲染
    Skinned     =10,   // 蒙皮渲染
    Count,
};


