


// 地形着色器,
// 处理地形混合



#include "common.hlsl"// 包含常用的函数和结构体定义

// 地形常量缓冲区
cbuffer TerrainCB : register(b2)
{
    float2 gTerrainCenter;
    float2 gTerrainSize;
    float2 gTerrainTexSize;
    float2 gTerrainTexScale;
    float  gTerrainScale;
    float  gTerrainMaxDist;
    float  gTerrainMaxTess;
    float  gTerrainMinTess;

}

Texture2D gHeightMap : register(t1,space1); //高度图纹理

// 地形的顶点输入,用于patch细分
struct PatchVertex
{
    float2 PosXZ : POSITION;
};

// ===== 曲面细分着色器结构体 =====
// 常量外壳着色器输出
struct HSConstOut
{
    float EdgeTess[4]   : SV_TessFactor;       // 边缘细分因子
    float InsideTess[2] : SV_InsideTessFactor; // 内部细分因子
};

// 域着色器输出
struct DSOut
{
    float4 PosH     : SV_POSITION;
    float3 PosW     : TEXCOORD0;
    float3 NormalW  : TEXCOORD1;
    float3 TangentW : TEXCOORD2;
    float2 UV       : TEXCOORD3;
};


// 辅助函数,计算地形高度
float SampleHeight(float2 worldXZ)
{
    // (worldXZ - gTerrainCenter) / gTerrainSize取值(-0.5,0.5)
    float uv = (worldXZ - gTerrainCenter) / gTerrainSize + 0.5f;
    uv = saturate(uv);
    return gHeightMap.Sample(gSamplerLinearClamp, uv).r * gTerrainScale;

}

// 辅助函数,计算地形法线
float3 SampleNormal(float2 worldXZ)
{
    float dx = gTerrainSize.x / gTerrainTexSize.x;
    float dz = gTerrainSize.y / gTerrainTexSize.y;
    
    float hL = SampleHeight(worldXZ - float2(dx, 0));
    float hR = SampleHeight(worldXZ + float2(dx, 0));
    float hT = SampleHeight(worldXZ - float2(0, dz));
    float hB = SampleHeight(worldXZ + float2(0, dz));
    
    return normalize(float3((hL - hR) / dx, 2.0f, (hT - hB) / dz));
}

// 顶点着色器只负责传递4个角点的XZ坐标
// 到曲面细分阶段,不做任何处理
PatchVertex VS(PatchVertex vin)
{
    return vin;
}

// 常量外壳着色器
// 接收4个顶点,输出4个外部细分,两个内部细分
HSConstOut HSConstant(InputPatch<PatchVertex, 4> patch)
{
    HSConstOut cout;
    
    // patch中心点
    float2 center = 0.25f * (
        patch[0].PosXZ + patch[1].PosXZ +
        patch[2].PosXZ + patch[3].PosXZ);
    
    //计算距离
    float2 d = center - gEyePosW.xz;
    float dist = length(d);
    
    //细分因子插值(越近越大)
    float t = saturate(1.0f - dist / gTerrainMaxDist);
    float tess = lerp(gTerrainMinTess, gTerrainMaxTess, t);
    
    //4外部
    cout.EdgeTess[0]   = tess;
    cout.EdgeTess[1]   = tess;
    cout.EdgeTess[2]   = tess;
    cout.EdgeTess[3]   = tess;
    //2内部
    cout.InsideTess[0] = tess;
    cout.InsideTess[1] = tess;
    
    return cout;
}

// 控制点外壳着色器
[domain("quad")] //四边形
[partitioning("fractional_even")] //分段方式,偶数分段,消除裂缝
[outputtopology("triangle_cw")] //输出拓扑,顺时针三角形
[outputcontrolpoints("HSConstant")] //输出控制点数,与HSConstant一致
PatchVertex HS(InputPatch<PatchVertex, 4> patch,
                   uint i:SV_outputControlPointID)
{
    return patch[i];
}

// 域着色器
[domain("quad")] //四边形
DSOut DS(HSConstOut hsConst,
        float2 uv:SV_DomainLocation,
         const OutputPatch<PatchVertex, 4> patch)
{
    DSOut dout;
    
    // 计算世界坐标
    // 双线性插值
    float2 bot = lerp(patch[0].PosXZ, patch[1].PosXZ, uv.x);
    float2 top = lerp(patch[3].PosXZ, patch[2].PosXZ, uv.x);
    float2 posXZ = lerp(bot, top, uv.y);
    
    // 计算高度
    float h = SampleHeight(posXZ);
    //位置
    dout.PosW = float3(posXZ.x, h, posXZ.y);
    //法线
    dout.NormalW = SampleNormal(posXZ);
    //切线
    float dx = gTerrainSize.x / gTerrainTexSize.x;
    float hL = SampleHeight(posXZ - float2(dx, 0));
    float hR = SampleHeight(posXZ + float2(dx, 0));
    dout.TangentW = normalize(float3(2.0f * dx, hR - hL, 0));
    //UV
    dout.UV = (posXZ - gTerrainCenter) / gTerrainSize + 0.5f;
    
    dout.PosH = mul(float4(dout.PosW, 1.0f), gViewProj);
    
    return dout;
}



float4 PS(DSOut pin) : SV_Target
{
    //从材质结构体缓冲区中拿对应的材质数据
    MaterialData matData = gMaterialData[gMaterialIndex];

    float4 diffAlbedo = matData.diffAlbedo;
    float3 fresnelR0 = matData.fresnelR0;
    float roughness = matData.roughness;
    
    float2 worldUV = pin.PosW.xz;
    float height = pin.PosW.y;


    float slope = 1.0f - pin.NormalW.y; // 0 = 平地, 1 = 垂直墙面

    
   
    float hLow = 1.0f;  // 高度带分界
    float blend = 1.0f; // 过度带

    // 分界线扰动,双波叠加
    float edgeNoise = terrainNoise(worldUV * 0.5f) * 0.7f 
                    +terrainNoise(worldUV * 1.3f) * 0.3f;
    float hLowJitter = hLow + (edgeNoise - 0.5f) * 2.0f;

    //平滑过渡带
    float t = smoothstep(hLowJitter - blend, hLowJitter + blend, height);

    //坡度过度
    float wSandSteep = smoothstep(0.55f, 0.70f, slope);
    
    //低处
    float wGravel = (1.0f - t) * (1.0f - wSandSteep);

    // 高处
    float wGrassBase = t * (1.0f - wSandSteep);

    // 草地中夹杂斑块
    // 用两层噪声叠加，让斑块边缘更碎
    float patchNoise = terrainNoise(worldUV * 0.4f + float2(11.0f, 7.0f)) * 0.65f
                 + terrainNoise(worldUV * 0.9f + float2(23.0f, 41.0f)) * 0.35f;

    // 斑块边缘变化块,阈值区间越窄，边缘越硬
    float sandPatch = smoothstep(0.58f, 0.66f, patchNoise);

    // 斑块只在平缓草地出现,让斑块和草地混合
    float wSandPatch = wGrassBase * sandPatch;
    float wGrass = wGrassBase * (1.0f - sandPatch);


    // 草地两张混合,让两张的地纹理也是"块状"分布
    float grassMix = terrainNoise(worldUV * 0.25f + float2(3.0f, 17.0f));
    grassMix = smoothstep(0.40f, 0.60f, grassMix);

    float wGrass1 = wGrass * grassMix;
    float wGrass2 = wGrass * (1.0f - grassMix);

    // 计算总权重
    float wSandTotal = wSandSteep + wSandPatch;
    float4 weights = float4(wSandTotal, wGravel, wGrass1, wGrass2);
    weights /= max(dot(weights, 1.0f), 0.001f);


    //层内微小扰动
    float4 layerNoise = float4(
    terrainNoise(worldUV * 0.9f + float2(0.0f, 0.0f)),
    terrainNoise(worldUV * 1.1f + float2(17.0f, 3.0f)),
    terrainNoise(worldUV * 0.7f + float2(5.0f, 29.0f)),
    terrainNoise(worldUV * 1.3f + float2(41.0f, 11.0f))
    );
    weights *= lerp(0.90f, 1.10f, layerNoise);
    weights /= max(dot(weights, 1.0f), 0.001f);


    diffAlbedo *= SampleDiffuse(matData, worldUV, 0, weights);

    float3 N = normalize(pin.NormalW);
    float3 T = normalize(pin.TangentW);
    float3 normalS = SampleNormalT(matData, worldUV, 0, weights);
    if (dot(normalS, normalS) > 1e-6f)
        N = NormalTToW(normalS, N, T);

    //ALPHA测试宏定义了就开启
#ifdef ALPHA_TEST
    //与漫反射颜色的a通道做比较,小于0.1裁剪掉
	clip(diffAlbedo.a - 0.1f);
#endif
    
    //反射平面裁切
    if (gIsClipPlaneEnabled)
    {
        float dist = dot(pin.PosW, gClipPlane.xyz) + gClipPlane.w; //点到平面的距离
        clip(dist); //小于0裁剪掉
    }

    //视线向量(从片元指向摄像机)
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    
    //环境光(*漫反射反照率)
    float4 ambient = gAmbientLight * diffAlbedo;

    // 光滑度
    const float shininess = 1.0f - roughness;
    //材质结构体,用于传递给光照计算函数
    Material mat = { diffAlbedo, fresnelR0, shininess };
    float3 shadowFactor = 1.0f; //先不计算阴影
    
    //计算直接光照,使用世界空间法线贴图
    float4 directLight = ComputeLight(gLights, mat, pin.PosW,
        N, toEyeW, shadowFactor);
    
    //间接光漫反射(环境光)+环境光镜面反射+直接光照
    float4 litColor = directLight + ambient;
    
    litColor.rgb *= lerp(1.0f, 0.5f, roughness * 0.5f);
    
    litColor.a = diffAlbedo.a; //没有开启AlphaTest默认用材质的透明度
    
    return litColor;

}
