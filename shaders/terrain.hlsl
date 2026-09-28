


// 地形着色器,
// 处理地形混合



#include "common.hlsl"// 经验光照计算着色器

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 UV : TEXCOORD;
    float3 TangentL : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : TEXCOORD0;
    float3 NormalW : TEXCOORD1;
    float3 TangentW : TEXCOORD2;
    float2 UV : TEXCOORD3;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f; //初始化
    
    //->世界空间位置
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW;
    
    //->世界空间法线(线性变换直接乘),非线性要乘逆转置
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    
    //->世界空间切线
    vout.TangentW = mul(vin.TangentL, (float3x3) gWorld);
    //->齐次裁剪空间位置
    vout.PosH = mul(posW, gViewProj);
    
    float4 uv = mul(float4(vin.UV, 0.0f, 1.0f), gTexTransform); //对象的纹理变换
    
    //从材质结构体缓冲区中拿对应的材质数据
    vout.UV = uv.xy;
    if (gMaterialIndex != -1)
    {
        MaterialData matData = gMaterialData[gMaterialIndex];
        vout.UV = mul(uv, matData.matTransform).xy; //材质的纹理变换
    }
    
    return vout;

}

float4 PS(VertexOut pin) : SV_Target
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
