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
};

static const float PI = 3.14159265359;

PSInput VSMain(VertexIn In)
{
    PSInput output;
    output.position = mul(float4(In.Position, 1.0f), gWorldViewProj);

    return output;
}

void PSMain(PSInput input)
{

}