cbuffer cbPerFrame : register(b0)
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
    
    float4x4 gInvViewProj;
};

Texture2D GBuffer0 : register(t0); // Albedo + Specular
Texture2D GBuffer1 : register(t1); // Normal + Roughness
Texture2D GBuffer2 : register(t2); 
Texture2D GBuffer3 : register(t3); 

Texture2D g_ShadowMask : register(t4);
TextureCube g_IrradianceMap : register(t5);
TextureCube g_PrefilteredMap : register(t6);
Texture2D g_BRDFLUT : register(t7);
Texture2D g_SceneDepth : register(t8);

SamplerState g_PointSampler : register(s0);
SamplerState g_ShadowMaskSampler : register(s1);
SamplerState g_LinearClampedSampler : register(s2);
struct PSInput
{
    float4 position : SV_POSITION; // [-1 1]
    float2 texCoord : TEXCOORD;    // uv[0 1

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

// ----------------------------------------------------------------------------
// IBL 专用的菲涅尔近似 (考虑粗糙度衰减)
// ----------------------------------------------------------------------------
float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(1.0 - roughness, F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

PSInput VSMain(uint vid : SV_VertexID)
{
    PSInput output;
    output.texCoord = float2((vid << 1) & 2, vid & 2);
    output.position = float4(output.texCoord.x * 2.0 - 1.0, output.texCoord.y * -2.0 + 1.0, 0.0, 1.0);
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float3  albedo       = GBuffer0.Sample(g_PointSampler, input.texCoord).rgb;
    float   roughness    = GBuffer0.Sample(g_PointSampler, input.texCoord).a;
    float3  N            = GBuffer1.Sample(g_PointSampler, input.texCoord).rgb;
    float   metallic     = GBuffer1.Sample(g_PointSampler, input.texCoord).a;
    float3  emissive     = GBuffer3.Sample(g_PointSampler, input.texCoord).rgb;
    float   ao           = GBuffer2.Sample(g_PointSampler, input.texCoord).a;
    
    
    // depth to ndc
    float depth = g_SceneDepth.SampleLevel(g_PointSampler, input.texCoord.xy, 0).r;
    
    float4 ndcPos;
    ndcPos.x = input.texCoord.x * 2.0 - 1.0;
    ndcPos.y = (1.0 - input.texCoord.y) * 2.0 - 1.0;
    ndcPos.z = depth;
    ndcPos.w = 1.0;
    
    float4 worldPosH = mul(ndcPos, gInvViewProj);
    float3 worldPos = worldPosH.xyz / worldPosH.w; // 必须除以 w，且是 float3
    
    float3 V = normalize(gCameraPos - worldPos);
    float3 R = reflect(-V, N);
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
    float3 specularDirect = numerator / denominator;
    
    float3 kS_Direct = F;
    float3 kD_Direct = float3(1.0, 1.0, 1.0) - kS_Direct;

    kD_Direct *= 1.0 - metallic;
    float NdotL = max(dot(N, L), 0.0);
    // Lo
    float3 Lo = (kD_Direct * albedo / PI + specularDirect) * gLightColor * NdotL;
    // Lo = float3(0.0, 0.0, 0.0);

    // IBL Fernle
    float3 kS_IBL = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    float3 kD_IBL = 1.0 - kS_IBL;
    kD_IBL *= (1.0 - metallic);
    // IBL diffuse
    float3 irradiance = g_IrradianceMap.Sample(g_LinearClampedSampler, N).rgb; // 应该用线性采样
    float3 diffuseIBL = irradiance * albedo;
    // Specular IBL
    const float MAX_REFLECTION_LOD = 4.0;
    float3 prefilteredColor = g_PrefilteredMap.SampleLevel(g_LinearClampedSampler, R, roughness * MAX_REFLECTION_LOD).rgb; // 应该用线性采样
    float2 brdf = g_BRDFLUT.Sample(g_LinearClampedSampler, float2(max(dot(N, V), 0.0), roughness)).rg;
    float3 specularIBL = prefilteredColor * (kS_IBL * brdf.x + brdf.y);
    
    
    float3 ambient = (kD_IBL * diffuseIBL + specularIBL) * ao;
    // float3 ambient = (specularIBL) * ao;
    // float3 ambient = float3(0.03, 0.03, 0.03) * albedo;
    // ambient = float3(0.0, 0.0, 0.0);
    
    float3 color = ambient + Lo;
    
    
    int3 screenPos = int3(input.position.xy, 0);

    float shadowFactor = g_ShadowMask.Load(screenPos).r;

    // 记得加上自发光 emissive!
    float3 finalColor = ambient + (Lo * shadowFactor);
    finalColor = pow(finalColor, 1.0 / 2.2);
    return float4(finalColor, 1.0);
}