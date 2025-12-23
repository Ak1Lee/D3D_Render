cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
    float4x4 gWorld;
};
cbuffer cbPerFrame : register(b1)
{
    float3 gLightDir;
    float gLightIntensity;
    
    float3 gLightColor;
    float _Padding1;
    
    float3 gAmbientColor;
    float _Padding2;
    
    float3 gCameraPos;
    float _Padding3;
    
    float4x4 gLightViewProj;
};

cbuffer MaterialCB : register(b2)
{
    float4 g_Albedo; // 颜色
    float g_Roughness; // 粗糙度
    float g_Metallic; // 金属度
    float g_AO; // 环境光遮蔽
    float g_Padding; // 凑齐 16 字节对齐
};

Texture2D g_ShadowMap : register(t0); // 对应 Slot 3
SamplerState g_samShadow : register(s0); // 比较采样器 (Comparison Sampler)


struct VertexIn
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD; 
    float3 Tangent : TANGENT;
    float4 Color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 tangent : TANGENT;
    float4 color : COLOR;
    float3 worldposition : WORLDPOSITION;
    
    float4 PosLightSpace : POSINLIGHT;
};

static const float PI = 3.14159265359;

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001);
}
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0; // 直接光照下的k值计算

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

PSInput VSMain(VertexIn In)
{
    PSInput output;
    output.position = mul(float4(In.Position, 1.0f), gWorldViewProj);
    output.normal = In.Normal;
    output.texCoord = In.TexCoord;
    output.tangent = In.Tangent;
    output.color = In.Color;
    output.color.rgb = In.Normal;
    output.color.rgb = g_Albedo.rgb;
    output.color.rgb = g_Albedo.rgb * dot(In.Normal, gLightDir) * 0.5 + 0.5;
    float4 posW = mul(float4(In.Position, 1.0f), gWorld);
    output.worldposition = posW;
    output.PosLightSpace = mul(posW, gLightViewProj);
    //gLightColor; // 简单光照调节
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    
    // Shadow
    float3 ProjCoords = input.PosLightSpace.xyz / input.PosLightSpace.w;
    ProjCoords.x = ProjCoords.x * 0.5 + 0.5;
    ProjCoords.y = 1-(ProjCoords.y * 0.5 + 0.5);
    // ProjCoords.z = ProjCoords.z * 0.5 + 0.5;

    
    float closestDepth = g_ShadowMap.Sample(g_samShadow, ProjCoords.xy).r;
    float currentDepth = ProjCoords.z;
    
    float bias = 0.005;
    
    float2 texelSize = 1.0 / 2048.0;
    float shadow = 0.0;
    // PCF 3x3 循环
    // 也就是采样当前像素周围一圈的 9 个点
    for (int x = -2; x <= 2; ++x)
    {
        for (int y = -2; y <= 2; ++y)
        {
            // SampleCmpLevelZero 参数说明：
            // 1. 采样器
            // 2. 纹理坐标 (当前坐标 + 偏移量)
            // 3. 比较值 (当前像素深度)
            // 它会自动比较并返回 [0.0, 1.0] 之间的值 (0=黑, 1=亮, 中间值=边缘过度)
            closestDepth = g_ShadowMap.Sample(g_samShadow, ProjCoords.xy + float2(x, y) * texelSize).r;
            shadow += (currentDepth - bias) > closestDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    
    
    return float4(shadow, shadow, shadow, 1.0);
    // return float4(1.0, 0.0, 0.0, 1.0);
    
}