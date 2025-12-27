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
    float gLightSize;
    
    float3 gAmbientColor;
    float _Padding2;
    
    float3 gCameraPos;
    float _Padding3;
    
    float4x4 gLightViewProj;
};

cbuffer MaterialCB : register(b2)
{
    float4 g_Albedo; // ��ɫ
    float g_Roughness; // �ֲڶ�
    float g_Metallic; // ������
    float g_AO; // �������ڱ�
    float g_Padding; // ���� 16 �ֽڶ���
};

Texture2D g_ShadowMap : register(t0); // ��Ӧ Slot 3
SamplerState g_samShadow : register(s0); 
SamplerComparisonState g_samShadowCompare : register(s1);

static const float2 poissonDisk[16] =
{
    //float2(0.0, 0.0),
    float2(-0.94201624, -0.39906216),
    float2(0.94558609, -0.76890725),
    float2(-0.094184101, -0.92938870),
    float2(0.34495938, 0.29387760),
    float2(-0.91588581, 0.45771432),
    float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845),
    float2(0.97484398, 0.75648379),
    float2(0.44323325, -0.97511554),
    float2(0.53742981, -0.47371072),
    float2(-0.26496911, -0.41893023),
    float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507),
    float2(-0.81409955, 0.91437590),
    float2(0.19984126, 0.78641367),
    float2(0.14383161, -0.14100790)
};
float Rand(float2 co)
{
    return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
}


void FindBlocker(float2 uv, float zReceiver, float SearchRadius, out float avgBlockerDepth, out float numBlockers)
{
    avgBlockerDepth = 0.0;
    numBlockers = 0.0;
    
    float Noise = Rand(uv*100.0f);
    float s = sin(Noise * 6.28);
    float c = cos(Noise * 6.28);
    float2x2 RotationMatrix = float2x2(c, -s, s, c);
    
    float zSample00 = g_ShadowMap.Sample(g_samShadow, uv).r;
    //if (zSample00 < zReceiver - 0.005)
    
    for(int i = 0; i < 16; ++i)
    {
        float2 offset = mul(RotationMatrix, poissonDisk[i]) * SearchRadius;
        float2 sampleUV = uv + offset;
        float zSample = g_ShadowMap.Sample(g_samShadow, sampleUV).r;
        
            if (zSample < zReceiver + 0.00)
        {
            avgBlockerDepth += zSample;
            numBlockers+=1.0;
        }
    }
    
    if(numBlockers > 0)
    {
        avgBlockerDepth /= numBlockers;
        // avgBlockerDepth /= 16;
    }
}
float PCSS(float4 posLightSpace, out float3 debugMessage)
{
    debugMessage = float3(1.0,1.0,1.0);
    float3 ProjCoords = posLightSpace.xyz / posLightSpace.w;
    float2 uv = ProjCoords.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;
    
    float zRec = ProjCoords.z;
    
    if(uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;
    
    float SearchRadius =  (1.0 / 2048.0) * 16.0;
    
    float AvgBlockerDepth = 0;
    float NumBlockers = 0;
    
    FindBlocker(uv, zRec, SearchRadius, AvgBlockerDepth, NumBlockers);
    
    if(NumBlockers < 1.0)
        return 1.0;

    
    //return (NumBlockers / 16.0);
    
    float PenumbraDepth = (zRec - AvgBlockerDepth);
    
    //if (PenumbraDepth < 0.001)return 0.0; else PenumbraDepth = PenumbraDepth - 0.001;
    float PenumbraRatio = PenumbraDepth * 1.0;
    // return PenumbraRatio;
    float PenumbraSize = PenumbraRatio * gLightSize*0.1;
    debugMessage.r = PenumbraSize * 20;
//    if (PenumbraSize < 0.02)
 //       return 0.0;
    
    
    
    PenumbraSize = clamp(PenumbraSize, 0.0, 0.3);
    // return PenumbraSize;
    

        
    
    // PCF
    float ShadowVis = 0.0;
    float Noise = Rand(uv * 100.0f);
    float s = sin(Noise * 6.28);
    float c = cos(Noise * 6.28);
    float2x2 RotationMatrix = float2x2(c, -s, s, c);
    float bias = 0.001;
    for(int i = 0; i < 16; ++i)
    {
        float2 offset = mul(RotationMatrix, poissonDisk[i]) * PenumbraSize;
        float2 sampleUV = uv + offset;
        float zSample = g_ShadowMap.Sample(g_samShadow, sampleUV).r;

//        ShadowVis += g_ShadowMap.SampleCmpLevelZero(
//            g_samShadowCompare,
//            sampleUV,
//            zRec - bias
//        );

        float isLit = (zSample >= zRec - bias) ? 1.0 : 0.0;


        ShadowVis += isLit;

    }
    // return PenumbraSize;
    // return PenumbraSize * 50.0;
    debugMessage.g = ShadowVis / 16.0;
    return ShadowVis / 16.0;
    
}

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
    float k = (r * r) / 8.0; // ֱ�ӹ����µ�kֵ����

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
    //gLightColor; // �򵥹��յ���
    return output;
}

// PCF
// float4 PSMain(PSInput input) : SV_TARGET
// {
    
//     // Shadow
//     float3 ProjCoords = input.PosLightSpace.xyz / input.PosLightSpace.w;
//     ProjCoords.x = ProjCoords.x * 0.5 + 0.5;
//     ProjCoords.y = 1-(ProjCoords.y * 0.5 + 0.5);
//     // ProjCoords.z = ProjCoords.z * 0.5 + 0.5;

    
//     float closestDepth = g_ShadowMap.Sample(g_samShadow, ProjCoords.xy).r;
//     float currentDepth = ProjCoords.z;
    
//     float bias = 0.005;
    
//     float2 texelSize = 1.0 / 2048.0;
//     float shadow = 0.0;
//     // PCF 3x3 ѭ��
//     // Ҳ���ǲ�����ǰ������ΧһȦ�� 9 ����
//     for (int x = -2; x <= 2; ++x)
//     {
//         for (int y = -2; y <= 2; ++y)
//         {
//             // SampleCmpLevelZero ����˵����
//             // 1. ������
//             // 2. �������� (��ǰ���� + ƫ����)
//             // 3. �Ƚ�ֵ (��ǰ�������)
//             // �����Զ��Ƚϲ����� [0.0, 1.0] ֮���ֵ (0=��, 1=��, �м�ֵ=��Ե����)
//             closestDepth = g_ShadowMap.Sample(g_samShadow, ProjCoords.xy + float2(x, y) * texelSize).r;
//             shadow += (currentDepth - bias) > closestDepth ? 0.0 : 1.0;
//         }
//     }
//     shadow /= 25.0;
    
    
//     return float4(shadow, shadow, shadow, 1.0);
//     // return float4(1.0, 0.0, 0.0, 1.0);
    
// }

// PCSS
float4 PSMain(PSInput input) : SV_TARGET
{
    float3 debugMessage = 0.0;
    
    float shadow = PCSS(input.PosLightSpace, debugMessage);
    //shadow = 1 - shadow;
    
    //return float4(debugMessage, 1.0);
    return float4(shadow, shadow, shadow, 1.0);
    // return float4(1.0, 0.0, 0.0, 1.0);
    
}