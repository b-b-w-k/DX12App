//common.hlsl 
//包含着色器文件的公共部分
//常量缓冲区...着色器资源
//灯光数量定义
//公共函数

#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS   1
#endif
#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 1
#endif
#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS  0
#endif

#include "light.hlsl"//经验光照计算着色器

// 材质层级结构体
struct MaterialLayerData
{
    int    diffMapIndex;
    int    norMapIndex;
    float  rotation;
    float  weight;
    float2 uvScale;
    float2 uvSpeed;
    
};

//材质结构体
struct MaterialData
{
    float4   diffAlbedo;
    float3   fresnelR0;
    float    roughness;
    float4x4 matTransform;
    MaterialLayerData matLayer[4];//包含最多4层纹理
    int matLayerCount;
    int matPad[3];
};

// 纹理资源数组,可以存放不同大小的纹理资源
// 静态立方体纹理
TextureCube gCubeTexMap : register(t0, space0);

// 材质结构体缓冲区,存放材质数据
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);

// t2是动态反射纹理(平面/立方体)按需提供

// 骨骼结构体缓冲区,存放骨骼最终变换矩阵
StructuredBuffer<float4x4> gBoneTransforms : register(t0, space3);

// 2D纹理资源
Texture2D   gTextureMap[4] : register(t1);



//静态采样器
SamplerState gSamplerPointWrap   : register(s0);
SamplerState gSamplerPointClamp  : register(s1);
SamplerState gSamplerLinearWrap  : register(s2);
SamplerState gSamplerLinearClamp : register(s3);
SamplerState gSamplerAnisoWrap   : register(s4);
SamplerState gSamplerAnisoClamp  : register(s5);

//对象常量缓冲区
cbuffer ObjectCB : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
    uint     gMaterialIndex;
    uint     gBoneOffset;
    uint     gObjPad1;
    uint     gObjPad2;
};

//过程常量缓冲区
cbuffer PassCB : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3   gEyePosW;
    int      gIsClipPlaneEnabled; //裁剪平面是否启用
    float2   gRenderTargetSize;
    float2   gInvRenderTargetSize;
    float    gNearZ;
    float    gFarZ;
    float    gTotalTime;
    float    gDeltaTime;
    
    float4   gAmbientLight; //环境光
    Light    gLights[MAX_LIGHTS]; //光照数据
    
    float4   gClipPlane; //裁剪平面
};

//法线贴图切线空间->世界空间
float3 NormalTToW(float3 normalSample, float3 NormalW, float3 tangentW)
{
    float3 N = NormalW;
     //让T,N保持正交 ,T减去(T在N方向上的分量)
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);
    
     //构建TBN规范正交基
    float3x3 TBN = float3x3(T, B, N);
    
    //切线空间转世界空间
    return normalize(mul(normalSample, TBN));
}

float3 VectorWToT(float3 vectorW,float normalW,float tangentW)
{
    float3 N = normalW;
     //让T,N保持正交 ,T减去(T在N方向上的分量)
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);
    
    //TBN 是正交矩阵 逆矩阵=转置矩阵
    float3x3 TBN_transpose = float3x3(  T.x, B.x, N.x,
                                        T.y, B.y, N.y,
                                        T.z, B.z, N.z);
    //世界空间转切线空间
    return normalize(mul(vectorW, TBN_transpose));

}

// 获取纹理层级UV
float2 GetLayerUV(float2 baseUV, MaterialLayerData layer,float time)
{
    float s, c;
    sincos(layer.rotation, s, c); //计算旋转角度
    float2x2 R = float2x2(c, -s, s, c);
    float2 ruv = mul(R, baseUV); // 旋转
    return ruv * layer.uvScale + layer.uvSpeed * time;
}

//采样漫反射纹理
float4 SampleDiffuse(MaterialData mat, float2 baseUV, float time, float4 weight)
{
    float4 colorSum  = 0; // 总颜色
    float  weightSum = 0; // 总权重
    
    //循环次数
    int count = min(mat.matLayerCount, 4);
    
    [unroll]//循环展开
    for (int i = 0; i < 4;++i)
    {
        if (i >= count) break;
        
        MaterialLayerData layer = mat.matLayer[i];
        float w = weight[i] >= 0.0f ? weight[i] : layer.weight;
        if (w <= 0.0f)continue;
        if (layer.diffMapIndex == -1) continue;
        
        //获取uv
        float2 uv = GetLayerUV(baseUV, layer, time);
        
       
        
        colorSum += gTextureMap[layer.diffMapIndex]
                .Sample(gSamplerAnisoWrap, uv) * w;
        weightSum += w;

    }
    if (weightSum <= 0.0f)
        return float4(1,1,1,1); // 无纹理返回纯色
    //由于权重总和不一定为1,要取平均数
    return colorSum / weightSum;
    
}
//采样法线
float3 SampleNormalT(MaterialData mat, float2 baseUV, float time, float4 weight)
{
    float3 normalSum = 0; // 总颜色
    float weightSum = 0; // 总权重
    
    //循环次数
    int count = min(mat.matLayerCount, 4);
    
    [unroll]//循环展开
    for (int i = 0; i < 4; ++i)
    {
        if (i >= count) break;
        
        MaterialLayerData layer = mat.matLayer[i];
        float w = weight[i] >= 0.0f ? weight[i] : layer.weight;
        if (w <= 0.0f) continue;
        if (layer.norMapIndex == -1)continue;
        
        //获取uv
        float2 uv = GetLayerUV(baseUV, layer, time);
        
        float3 n = gTextureMap[layer.norMapIndex]
                .Sample(gSamplerAnisoWrap, uv).rgb * 2.0f - 1.0f;
        
        normalSum += n * w;
        weightSum += w;

    }
    if (weightSum <= 0.0f)
        return float3(0,0,0); //外部使用默认法线
    //由于权重总和不一定为1,要取平均数
    return normalSum / weightSum;
}

//噪声扰动函数
float terrainNoise(float2 p)
{
    float n = 0.0f;
    n += sin(p.x * 0.13f) * cos(p.y * 0.17f) * 0.5f;
    n += sin(p.x * 0.31f) * cos(p.y * 0.27f) * 0.3f;
    n += sin(p.x * 0.71f) * cos(p.y * 0.63f) * 0.2f;
    return saturate(n * 0.5f + 0.5f); // 归一化到 0~1
}
