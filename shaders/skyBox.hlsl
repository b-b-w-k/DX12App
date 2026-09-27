
//天空盒渲染着色器
//使用立方体贴图绘制环境天空

#include"common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 UV : TEXCOORD;

};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : TEXCOORD0;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.PosL = vin.PosL;
    //->世界空间
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    //以摄像机(观测点)为中心
    posW.xyz += gEyePosW;
    //将w=1,让其始终在最远处
    vout.PosH = mul(posW, gViewProj).xyww;
    return vout;

}

float4 PS(VertexOut pin) : SV_Target
{
    //使用本地坐标采样
    return gCubeTexMap.Sample(gSamplerLinearWrap, pin.PosL);

}