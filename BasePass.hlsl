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
    float4 g_Albedo;
    float g_Roughness;
    float g_Metallic;
    float g_AO;
    float g_Padding;
};

Texture2D g_ShadowMap : register(t0);
Texture2D g_ShadowMask : register(t1);

SamplerState g_samShadow : register(s0); //(Comparison Sampler)
SamplerState g_Sampler : register(s2); // Linear Wrap (CubeMap)
SamplerState g_ClampedSampler : register(s3); // Linear Clamp ( LUT)


TextureCube g_IrradianceMap : register(t10);
TextureCube g_PrefilterMap : register(t11);
Texture2D g_BRDFLUT : register(t12);
Texture2D g_AlbedoMap : register(t13);
Texture2D g_NormalMap : register(t14);
Texture2D g_MetallicRoughnessMap : register(t15);


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

struct PSOutput
{
    float4 AlbedoRoughness : SV_Target0;
    float4 NormalMetallic  : SV_Target1;
    float4 Emissive        : SV_Target2;
    
};

PSInput VSMain(VertexIn In)
{
    PSInput output;
    output.position = mul(float4(In.Position, 1.0f), gWorldViewProj);
    // output.normal = In.Normal;
    output.normal = normalize(mul(In.Normal, (float3x3) gWorld));
    output.texCoord = In.TexCoord;
    output.tangent = In.Tangent;
    output.color = In.Color;
    float4 posW = mul(float4(In.Position, 1.0f), gWorld);
    output.worldposition = posW;
    output.PosLightSpace = mul(posW, gLightViewProj);
    return output;
}

PSOutput PSMain(PSInput In)
{
    PSOutput output;
    float3 albedo = g_AlbedoMap.Sample(g_Sampler, In.texCoord).rgb * g_Albedo.rgb;
    float3 normal = In.normal;
    // float metallic = g_MetallicRoughnessMap.Sample(g_Sampler, In.texCoord).b * g_Metallic;
    float metallic = g_Metallic;
    
    // float roughness = g_MetallicRoughnessMap.Sample(g_Sampler, In.texCoord).g * g_Roughness;
    float roughness = g_Roughness;
    
    output.AlbedoRoughness = float4(albedo, roughness);
    output.NormalMetallic = float4(normalize(normal), metallic);
    output.Emissive = float4(0, 0, 0, 1);
    
    return output;
}