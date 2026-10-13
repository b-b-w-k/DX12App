
// 标准着色器,
// 只处理漫反射纹理和法线纹理
// 提供动态环境反射效果
// 提供骨骼动画


#include "common.hlsl"// 经验光照计算着色器


// 动态反射立方体纹理
TextureCube gEnvCubeMap : register(t0, space2);

struct VertexIn
{
    float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
    float2 UV      : TEXCOORD;
    float3 TangentL: TANGENT;
#ifdef  SKINNED  //有骨骼时输入结构体
    float4 BoneWeights : BONEWEIGHTS;
    uint4  BoneIndices : BONEINDICES;
#endif
};

struct VertexOut
{
    float4 PosH     : SV_POSITION;
    float3 PosW     : TEXCOORD0;
    float3 NormalW  : TEXCOORD1;
    float3 TangentW : TEXCOORD2;
    float2 UV       : TEXCOORD3;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;//初始化
    
#ifdef  SKINNED //有骨骼时顶点更改
    
    //获取权重
    float weights[4] = { 0, 0, 0, 0 };
    weights[0] = vin.BoneWeights.x;
    weights[1] = vin.BoneWeights.y;
    weights[2] = vin.BoneWeights.z;
    // 归一化
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];
    
    float3 posL = float3(0.0f, 0.0f, 0.0f);
    float3 normalL = float3(0.0f, 0.0f, 0.0f);
    float3 tangentL = float3(0.0f, 0.0f, 0.0f);
    
    [unroll]// 建议展开
    for (int i = 0; i < 4; ++i)
    {
        //全局骨骼索引
        uint gBoneIndex = vin.BoneIndices[i] + gBoneOffset;
        //获取骨骼变换矩阵
        float4x4 boneTran = gBoneTransforms[gBoneIndex];
        
        //变换顶点数据
        posL += weights[i] * mul(float4(vin.PosL, 1.0f), boneTran).xyz;
        normalL += weights[i] * mul(vin.NormalL, (float3x3)boneTran);
        tangentL += weights[i] * mul(vin.TangentL, (float3x3) boneTran);
    }
    vin.PosL = posL;
    vin.NormalL = normalL;
    vin.TangentL = tangentL;
#endif
    
    //->世界空间位置
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW;
    
    //->世界空间法线(线性变换直接乘),非线性要乘逆转置
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    
    //->世界空间切线
    vout.TangentW = mul(vin.TangentL, (float3x3) gWorld);
    //->齐次裁剪空间位置
    vout.PosH = mul(posW, gViewProj);
    
    float4 uv = mul(float4(vin.UV, 0.0f, 1.0f), gTexTransform);//对象的纹理变换
    
    //从材质结构体缓冲区中拿对应的材质数据
    vout.UV = uv.xy;
    if (gMaterialIndex != -1)
    {
        MaterialData matData = gMaterialData[gMaterialIndex];
        vout.UV = mul(uv, matData.matTransform).xy; //材质的纹理变换
    }
    
    return vout;

}

float4 PS(VertexOut pin):SV_Target
{
    //从材质结构体缓冲区中拿对应的材质数据
    MaterialData matData = gMaterialData[gMaterialIndex];

    float4 diffAlbedo = matData.diffAlbedo;
    float3 fresnelR0 = matData.fresnelR0;
    float roughness = matData.roughness;
    
    
    //采样漫反射贴图,如果没有贴图就使用材质的漫反射颜色
    diffAlbedo *= SampleDiffuse(matData, pin.UV, 0, float4(-1, -1, -1, -1));
    
    
    //采样法线贴图,如果没有贴图就使用顶点法线
    float3 N = normalize(pin.NormalW);
    float3 T = normalize(pin.TangentW);
    float3 normalS = SampleNormalT(matData, pin.UV, 0, float4(-1, -1, -1, -1));
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
    
    //阴影因子计算
    float3 shadowFactor;
    shadowFactor[0] = CalcShadowFactor(mul(float4(pin.PosW, 1.0f), gShadowTransform)); //只受方向光影响
    shadowFactor[1] = 1.0f;//点光
    shadowFactor[2] = 1.0f;//聚光灯
    
    //计算直接光照,使用世界空间法线贴图
    float4 directLight = ComputeLight(gLights, mat, pin.PosW,
        N, toEyeW, shadowFactor);
    
    //间接光漫反射(环境光)+环境光镜面反射+直接光照
    float4 litColor = directLight + ambient;
    
    //反射
    float3 r = reflect(-toEyeW, N);
    float3 envColor = gEnvCubeMap.Sample(gSamplerLinearWrap, r).rgb; //环境光反射
    
    float3 F = SchlickFresnel(fresnelR0, N, toEyeW);
    float3 specEnv = F * envColor * shininess;
    
    litColor.rgb += specEnv;

    litColor.rgb *= lerp(1.0f, 0.5f, roughness * 0.5f);
    
    litColor.a = diffAlbedo.a;//没有开启AlphaTest默认用材质的透明度
    
    return litColor;

}
