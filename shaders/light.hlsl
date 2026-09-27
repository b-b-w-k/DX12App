
#define MAX_LIGHTS 16 //最大光源数量


struct Light
{
    float3 power;
    float decayStart;
    float3 direction;
    float decayEnd;
    float3 position;
    float spotPower;
};

struct Material
{
    float4 diffAlbedo;
    float3 fresnelR0;
    float  shininess;
};

//计算衰减
float CalDecay(float d, float start, float end)
{
    //衰减距离/总距离 返回(0-1)
    return saturate((end - d) / (end - start));

}
//石里克近似 菲涅尔方程 R0物质折射率
//反射百分比 =折射率+(1-折射率)*(1-入射角)^5;
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
    float cosThita = saturate(dot(normal, lightVec));
    float f0 = 1 - cosThita;
    float3 reflect = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);
    return reflect;

}
//BlinnPhong光照经验模型
//litColor=ambineLight(环境光)+diffLight(漫反射光)+SpecularLight(镜面反射光,高光)
//
float3 BlinnPhong(float3 lightPower, float3 lightVec, float3 normal, float3 toEye,Material mat)
{
    const float m = mat.shininess * 256.0f; //粗糙度
    float3 halfVec = normalize(toEye + lightVec);//中间向量(半角向量)
    
    //归一化因子:(m + 8.0f) / 8.0f
    //粗糙的因子:归一化因子*cos(半角向量与法线的夹角)^m
    //求取粗糙度因子(越光滑高光越强烈,影响镜面高光)
    float rouFactor = (m + 8.0f) / 8.0f * pow(max(dot(halfVec, normal), 0.0f), m);
    //求菲涅尔因子(被折射掉的光量)
    float3 fresnelFactor = SchlickFresnel(mat.fresnelR0, normal, lightVec);
    
    //镜面高光因子
    float3 specAlbedo = fresnelFactor * rouFactor;
    //将大于1的部分压缩到0-1之间,用于LDR色彩范围
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);
    
    //(漫反射+镜面高光)*强度
    return (mat.diffAlbedo.rgb + specAlbedo) * lightPower;

}

//方向光
float3 DirectionLight(Light L,Material mat,float3 normal,float3 toEye)
{
    float3 lightVec = normalize(-L.direction); //反射点朝向光源
    
    //朗伯余弦定律
    float nDotL = max(dot(normal, lightVec), 0.0f);//cosChiTa
    float3 lightPower = L.power * nDotL;//辐照度
    
    return BlinnPhong(lightPower, lightVec, normal, toEye, mat);

}

//点光

float3 PointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.position-pos; //目标点到光源的向量
    
    float d = length(lightVec);//光经过的路程
    
    if (d > L.decayEnd)//衰减范围外 不受光照影响
        return 0.0f;
    lightVec /= d; //归一化

    float nDotL = max(dot(normal, lightVec), 0.0f); //cosChiTa
    float3 lightPower = L.power * nDotL; //辐照度
    
    float decay = CalDecay(d, L.decayStart, L.decayEnd);
    lightPower *= decay;//乘上衰减
    
    return BlinnPhong(lightPower, lightVec, normal, toEye, mat);

}

//聚光灯
float3 SpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
    float3 lightVec = L.position - pos; //目标点到光源的向量
    
    float d = length(lightVec); //光经过的路程
    
    if (d > L.decayEnd)//衰减范围外 不受光照影响
        return 0.0f;
    lightVec /= d; //归一化
    
    float nDotL = max(dot(normal, lightVec), 0.0f); //cosChiTa
    float3 lightPower = L.power * nDotL; //辐照度
    
    float decay = CalDecay(d, L.decayStart, L.decayEnd);
    lightPower *= decay; //乘上衰减
    
    //照射范围限制
    //聚光灯衰减因子=衰减范围角^衰减强度
    float spotFactor = pow(max(dot(-lightVec, L.direction), 0.0f), mat.shininess);
    lightPower *= spotFactor;
    
    return BlinnPhong(lightPower, lightVec, normal, toEye, mat);
}

//复合光源计算

float4 ComputeLight(Light light[MAX_LIGHTS], Material mat,
                    float3 pos, float3 normal, float3 toEye,
                    float3 shadowFactor)
{
    float3 result = 0.0f;

    int i = 0;
    
#if(NUM_DIR_LIGHTS>0)//方向光
    for(i =0;i<NUM_DIR_LIGHTS;++i)
    {
        //方向光*对应阴影因子(在阴影里的不受光影响)
        result+=shadowFactor[i]*DirectionLight(light[i],mat,normal,toEye);
    }
#endif
    
#if(NUM_POINT_LIGHTS>0)//点光
    for(i =0;i<NUM_POINT_LIGHTS;++i)
    {
        result+=PointLight(light[NUM_DIR_LIGHTS + i],mat,pos,normal,toEye);
    }
#endif
    
#if(NUM_SPOT_LIGHTS>0)//聚光灯
    for(i =0;i<NUM_SPOT_LIGHTS;++i)
    {
        result+=SpotLight(light[NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + i],mat,pos,normal,toEye);
    }
#endif
    return float4(result, 0.0f);
}