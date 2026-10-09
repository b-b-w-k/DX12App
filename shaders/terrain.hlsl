


// 地形着色器,
// 处理地形混合



#include "common.hlsl"// 包含常用的函数和结构体定义

// 地形常量缓冲区
cbuffer TerrainCB : register(b2)
{
    float2 gTerrainSize;
    float2 gTerrainTexSize;
    float  gTerrainScale;
    float  terrainPad[3];

}

Texture2D gHeightMap : register(t1,space1); //高度图纹理

// 地形的顶点输入,用于patch细分
struct VertexIn
{
    float2 uv       : POSITION; 
    float2 c0       : PATCH_C0;   // 左上
    float2 c1       : PATCH_C1;   // 右上 
    float2 c2       : PATCH_C2;   // 右下 
    float2 c3       : PATCH_C3;   // 左下 
    float4 edgeTess : PATCH_TESS; // 细分因子
};
struct VertexOut
{
    float2 corner0  : TEXCOORD0;
    float2 corner1  : TEXCOORD1;
    float2 corner2  : TEXCOORD2;
    float2 corner3  : TEXCOORD3;
    float4 edgeTess : TEXCOORD4;
};

// ===== 曲面细分着色器结构体 =====
// 常量外壳着色器输出
struct HSConstOut
{
    float EdgeTess[4]   : SV_TessFactor;       // 边缘细分因子
    float InsideTess[2] : SV_InsideTessFactor; // 内部细分因子
};

// 域着色器输出(相当于原顶点着色器的输出)
struct DSOut
{
    float4 PosH     : SV_POSITION;
    float3 PosW     : TEXCOORD0;
    float3 NormalW  : TEXCOORD1;
    float3 TangentW : TEXCOORD2;
    float2 UV       : TEXCOORD3;
};


// 辅助函数,计算地形高度
float SampleHeight(float2 localXZ)
{
    float2 uv = localXZ / gTerrainSize + 0.5f;
    uv = saturate(uv);
    return gHeightMap.SampleLevel(gSamplerLinearClamp, uv,0).r * gTerrainScale;

}

// 辅助函数,计算地形法线
float3 SampleNormal(float2 localXZ, float stepX, float stepZ)
{
    
    float hL = SampleHeight(localXZ - float2(stepX, 0));
    float hR = SampleHeight(localXZ + float2(stepX, 0));
    float hT = SampleHeight(localXZ - float2(0, stepZ));
    float hB = SampleHeight(localXZ + float2(0, stepZ));
    
    float dhdx = (hR - hL) / (2.0f * stepX);
    float dhdz = (hB - hT) / (2.0f * stepZ);
    
    return normalize(float3(-dhdx, 1.0f, -dhdz));
}

// 顶点着色器只负责传递4个角点的XZ坐标
// 到曲面细分阶段,不做任何处理
VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.corner0  = vin.c0;
    vout.corner1  = vin.c1;
    vout.corner2  = vin.c2;
    vout.corner3  = vin.c3;
    vout.edgeTess = vin.edgeTess;
    return vout;
}

// 常量外壳着色器
// 接收4个顶点,输出4个外部细分,两个内部细分
HSConstOut HSConstant(InputPatch<VertexOut, 4> patch)
{
    HSConstOut cout;
    
    //四个角点tess相同
    float4 e = patch[0].edgeTess;
    
    //4外部
    cout.EdgeTess[0] = max(e.w, 1.0f);
    cout.EdgeTess[1] = max(e.x, 1.0f);
    cout.EdgeTess[2] = max(e.y, 1.0f);
    cout.EdgeTess[3] = max(e.z, 1.0f);
    //2内部
    float inner = min(min(cout.EdgeTess[0], cout.EdgeTess[1]),
	                  min(cout.EdgeTess[2], cout.EdgeTess[3]));
    cout.InsideTess[0] = inner;
    cout.InsideTess[1] = inner;
    
    return cout;
}

// 控制点外壳着色器
[domain("quad")] //四边形
[partitioning("fractional_even")] //分段方式,偶数分段,消除裂缝
[outputtopology("triangle_ccw")] //输出拓扑序
[outputcontrolpoints(4)] //输出控制点数,与HSConstant一致
[patchconstantfunc("HSConstant")] //指定常量外壳着色器
[maxtessfactor(64.0f)]
VertexOut HS(InputPatch<VertexOut, 4> patch,
                   uint i:SV_outputControlPointID)
{
    return patch[i];
}

// 域着色器
[domain("quad")] //四边形
DSOut DS(HSConstOut hsConst,
        float2 uv:SV_DomainLocation,
        const OutputPatch<VertexOut, 4> patch)
{
    DSOut dout;
    
    // 计算世界坐标
    // 双线性插值
    float2 bot = lerp(patch[0].corner0, patch[0].corner1, uv.x);
    float2 top = lerp(patch[0].corner3, patch[0].corner2, uv.x);
    float2 posXZ = lerp(bot, top, uv.y);
    
    // 计算高度
    float h = SampleHeight(posXZ);
    //位置
    float3 posL = float3(posXZ.x, h, posXZ.y);
    
    dout.PosW = mul(float4(posL, 1.0f), gWorld).xyz;
    
    //法线
    float texelX = gTerrainSize.x / gTerrainTexSize.x;
    float texelZ = gTerrainSize.y / gTerrainTexSize.y;
    float2 patchSize = float2(length(patch[0].corner1 - patch[0].corner0), // 沿 X 的边长
                              length(patch[0].corner3 - patch[0].corner0)); // 沿 Z 的边长
    //
    float stepX = max(patchSize.x / max(hsConst.InsideTess[0], 1.0f), texelX);
    float stepZ = max(patchSize.y / max(hsConst.InsideTess[1], 1.0f), texelZ);
    dout.NormalW = normalize(mul(float4(SampleNormal(posXZ, stepX, stepZ), 0.0f), gWorld).xyz);
    //切线
    float hL = SampleHeight(posXZ - float2(stepX, 0));
    float hR = SampleHeight(posXZ + float2(stepX, 0));
    float dhdx = (hR - hL) / (2.0f * stepX);
    float3 TangentL = normalize(float3(1.0f, dhdx, 0.0f));
    dout.TangentW = mul(float4(TangentL, 0.0f), gWorld).xyz;
    //UV
    dout.UV = posXZ / gTerrainSize + 0.5f;
    //裁剪空间位置
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
    

    float slope = 1.0f - normalize(pin.NormalW).y; // 0 = 平地, 1 = 垂直墙面

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
