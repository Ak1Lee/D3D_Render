TextureCube g_EnvironmentMap : register(t0);
RWTexture2DArray<float4> g_PrefilterMap : register(u0);
SamplerState g_Sampler : register(s0);

cbuffer ConstBuffer : register(b0)
{
    float g_Roughness;
};
static const float PI = 3.14159265359;
// --- 辅助函数：Hammersley 低差异序列 ---
// 生成 [0,1] 区间的均匀伪随机点
float2 Hammersley(uint i, uint N)
{
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float rdi = float(bits) * 2.3283064365386963e-10;
    return float2(float(i) / float(N), rdi);
}

// --- 辅助函数：GGX 重要性采样 ---
// 输入：随机点 Xi，法线 N，粗糙度 roughness
// 输出：一个偏向高光波瓣方向的半程向量 H (世界空间)
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    
    // 1. 球坐标转换 (根据 GGX NDF 概率分布反推)
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    // 2. 转换到局部切线空间坐标
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    
    // 3. 转换到世界空间 (构建 TBN 矩阵)
    float3 Up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 Tangent = normalize(cross(Up, N));
    float3 Bitangent = cross(N, Tangent);
    
    // Tangent Space -> World Space
    float3 sampleVec = Tangent * H.x + Bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

// --- 辅助函数：计算 CubeMap 面方向 ---
float3 GetDirection(uint faceIdx, float2 uv)
{
    float3 dir;
    switch (faceIdx)
    {
        case 0:
            dir = float3(1.0, uv.y, -uv.x);
            break; // +X
        case 1:
            dir = float3(-1.0, uv.y, uv.x);
            break; // -X
        case 2:
            dir = float3(uv.x, 1.0, -uv.y);
            break; // +Y
        case 3:
            dir = float3(uv.x, -1.0, uv.y);
            break; // -Y
        case 4:
            dir = float3(uv.x, uv.y, 1.0);
            break; // +Z
        case 5:
            dir = float3(-uv.x, uv.y, -1.0);
            break; // -Z
    }
    return normalize(dir);
}

// 线程组定义：每组处理 32x32 个像素，Z=1
[numthreads(32, 32, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint width, height, elements;
    g_PrefilterMap.GetDimensions(width, height, elements);
    if (DTid.x >= width || DTid.y >= height)
        return;
    
    float2 uv = (float2(DTid.xy) + 0.5) / float2(width, height);
    uv = uv * 2.0 - 1.0;
    uv.y = -uv.y;
    
    float3 N = GetDirection(DTid.z, uv);
    float3 R = N;
    float3 V = R;

    float3 prefilteredColor = float3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;
    
    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, g_Roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            float sourceMip = 0.0;
            if (g_Roughness > 0.1)
            {
                sourceMip = 1.0 + g_Roughness * 2.0;
            }
            float3 envColor = g_EnvironmentMap.SampleLevel(g_Sampler, L, sourceMip).rgb;
            float maxBrightness = 60.0f;
            envColor = min(envColor, maxBrightness);
            prefilteredColor += envColor * NdotL;
            totalWeight += NdotL;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;

    g_PrefilterMap[DTid] = float4(prefilteredColor, 1.0);
    
    // float3 uvw = float3((float2(DTid.xy) + 0.5f) / float2(width, height) * 2.0 - 1.0, 1.0);
    
    // g_PrefilterMap[DTid] = float4(g_Roughness, g_Roughness, g_Roughness, 1.0);

}