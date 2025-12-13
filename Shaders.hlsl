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

struct VertexIn
{
    float3 Position : POSITION; // 必须匹配
    float3 Normal : NORMAL; // 可以不用，但要声明保持顺序
    float2 TexCoord : TEXCOORD; // 可以不用，但要声明保持顺序
    float3 Tangent : TANGENT; // 可以不用，但要声明保持顺序
    float4 Color : COLOR; // 使用
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 tangent : TANGENT;
    float4 color : COLOR;
};

PSInput VSMain(VertexIn In)
{
    PSInput output;
    output.position = mul(float4(In.Position, 1.0f), gWorldViewProj);
    output.normal = In.Normal; // 传递（即使暂不使用）
    output.texCoord = In.TexCoord; // 传递
    output.tangent = In.Tangent; // 传递
    output.color = In.Color;
    output.color.rgb = In.Normal;
    //gLightColor; // 简单光照调节
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}