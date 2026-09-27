
//基础hlsl着色器 color.hlsl


#include"common.hlsl"

//顶点输入
struct VertexIn
{
    float3 PosL  : POSITION;
    float4 Color : COLOR;
};

//顶点输出->像素输入
struct VertexOut
{
    float4 PosH  : SV_POSITION;
    float4 Color : TEXCOORD0;
};

//顶点着色器
VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    //float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(float4(vin.PosL, 1.0f), gViewProj);
    
    vout.Color = vin.Color;
    return vout;
}

//像素(片元)着色器
float4 PS(VertexOut pin):SV_Target
{
    return pin.Color;
}