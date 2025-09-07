cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
    PSInput output;
    output.position = mul(float4(position), gWorldViewProj);
    // output.position = float4(position);
    
    output.color = color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}