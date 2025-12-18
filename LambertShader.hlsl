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
};

PSInput VSMain(VertexIn In)
{
    PSInput output;
    output.position = mul(float4(In.Position, 1.0f), gWorldViewProj);
    output.normal = In.Normal;
    output.texCoord = In.TexCoord;
    output.tangent = In.Tangent;
    output.color = In.Color;
    output.color.rgb = In.Normal;
    output.color.rgb = dot(In.Normal, gLightDir)*0.5 + 0.5;
    //gLightColor; // 简单光照调节
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}