//水面着色器
//动态水面波纹


#include "common.hlsl"

//动态平面反射纹理
Texture2D gPlanarMap : register(t0, space2);

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
    vout.PosH = mul(posW, gViewProj); //光栅化会自动除以w分量，得到屏幕空间位置
    
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
    // 材质 
    MaterialData matData = gMaterialData[gMaterialIndex];
    float4 diffAlbedo = matData.diffAlbedo;
    float3 fresnelR0 = matData.fresnelR0;
    float roughness = matData.roughness;
    
     //采样漫反射贴图,如果没有贴图就使用材质的漫反射颜色
    diffAlbedo *= SampleDiffuse(matData, pin.UV, gTotalTime, float4(-1, -1, -1, -1));
    
    
    //采样法线贴图,如果没有贴图就使用顶点法线
    float3 NorW = normalize(pin.NormalW);
    float3 T = normalize(pin.TangentW);
    float3 normalS = SampleNormalT(matData, pin.UV, gTotalTime, float4(-1, -1, -1, -1));
    float3 N = NorW;
    if (dot(normalS, normalS) > 1e-6f)
        N = NormalTToW(normalS, N, T);

     // 视线方向
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    float NdotV = saturate(dot(NorW, toEyeW));
    float grazingFactor = saturate(NdotV * 4.0f);//掠射因子

    // 屏幕空间UV坐标
    float2 screenUV = pin.PosH.xy * gInvRenderTargetSize;
    float2 uvOffset = (N.xy - NorW.xy) * 0.03f * grazingFactor;//反射扰动
    screenUV = saturate(screenUV + uvOffset);
    
    // 采样反射 RT 
    float3 reflectColor = gPlanarMap.Sample(gSamplerLinearWrap, screenUV).rgb;


    // 菲涅尔,掠射角反射更强 
    float3 F = SchlickFresnel(fresnelR0, N, toEyeW);
    float fresnel = saturate(F.r);
    
    // 水面颜色
    float3 waterColor = diffAlbedo.rgb;

    //两者混合
    float3 finalColor = lerp(waterColor, reflectColor, fresnel);

    // 光照 
    const float shininess = 1.0f - roughness;
    Material mat = { float4(waterColor, 1.0f), fresnelR0, shininess };
    float3 shadowFactor = 1.0f;

    float4 ambient = gAmbientLight * float4(waterColor, 1.0f) * 0.3f;
    float4 directLight = ComputeLight(gLights, mat, pin.PosW, N, toEyeW, shadowFactor) * (1 - diffAlbedo.a);

    finalColor += ambient.rgb + directLight.rgb;
    
    //让透明度随菲涅尔变化,掠射角更透明
    float alpha = saturate(diffAlbedo.a + (1.0f - diffAlbedo.a) * pow(fresnel, 0.3f));
    
    return float4(finalColor, alpha);
}