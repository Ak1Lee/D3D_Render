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
    float3 PosL : POSITION;
};

TextureCube g_CubeMap : register(t0);

SamplerState g_LinearSampler : register(s2);

PSInput VSMain(VertexIn In)
{
    PSInput output;
    float3 worldPos = In.Position;
    
    
    output.position = mul(float4(worldPos, 1.0f), gWorldViewProj);
    output.position.z = output.position.w;
    // output.position.z = 1.0;
    output.PosL = In.Position;
    return output;
}

float4 PSMain(PSInput pin) : SV_Target
{
    return g_CubeMap.Sample(g_LinearSampler, pin.PosL);
}