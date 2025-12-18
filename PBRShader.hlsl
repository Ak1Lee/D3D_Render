cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
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
};

cbuffer MaterialCB : register(b2)
{
    float4 g_Albedo; // 颜色
    float g_Roughness; // 粗糙度
    float g_Metallic; // 金属度
    float g_AO; // 环境光遮蔽
    float g_Padding; // 凑齐 16 字节对齐
};

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
    output.worldposition = In.Position;
    //gLightColor; // 简单光照调节
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3 albedo = g_Albedo;
    float metallic = g_Metallic;
    float roughness = g_Roughness;
    float ao = g_AO;
    
    float3 N = input.normal;
    float3 V = normalize(gCameraPos - input.worldposition);
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);
    float3 L = gLightDir;
    float3 H = normalize(V + L);
    
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    float3 radiance = gLightColor;
    
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // +0.0001 防止除零
    float3 specular = numerator / denominator;
    
    // 计算漫反射比例 kD
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    metallic = 0;
    kD *= 1.0 - metallic;
    

    float NdotL = max(dot(N, L), 0.0);
    float3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;
    
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo;
    
    float3 color = ambient + Lo;
    
    return float4(color, 1.0);;
    //return float4(N,1.0);
}