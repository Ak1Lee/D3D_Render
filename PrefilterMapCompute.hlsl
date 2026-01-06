TextureCube g_EnvironmentMap : register(t0);
RWTexture2DArray<float4> g_PrefilterMap : register(u0);
SamplerState g_Sampler : register(s0);

cbuffer ConstBuffer : register(b0)
{
    float g_Roughness;
};


static const float PI = 3.14159265359;
// 线程组定义：每组处理 32x32 个像素，Z=1
[numthreads(32, 32, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint width, height, elements;
    g_PrefilterMap.GetDimensions(width, height, elements);
    
    float3 uvw = float3((float2(DTid.xy) + 0.5f) / float2(width, height) * 2.0 - 1.0, 1.0);
    
    g_PrefilterMap[DTid] = float4(g_Roughness, g_Roughness, g_Roughness, 1.0);

}