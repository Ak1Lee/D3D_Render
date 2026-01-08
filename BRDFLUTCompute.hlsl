TextureCube g_EnvironmentMap : register(t0);
RWTexture2D<float2> g_LUT : register(u0);
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

// --- 3. 几何遮蔽函数 (Geometry Schlick-GGX) ---
// Direct Light: k = (a+1)^2 / 8
// IBL:          k = a^2 / 2
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
float2 IntegrateBRDF(float NdotV, float roughness)
{
    // 构建视线向量 V
    // 假设 V 在 XZ 平面上
    float3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0; // Scale
    float B = 0.0; // Bias

    float3 N = float3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024u;
    
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float G = GeometrySmith(N, V, L, roughness);
            
            // 下面是 Split Sum 的数学推导结果：
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    
    return float2(A, B);
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

// --- Main ---
[numthreads(32, 32, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    uint width, height;
    g_LUT.GetDimensions(width, height);
    
    if (DTid.x >= width || DTid.y >= height)
        return;

    // 归一化坐标作为输入参数
    // DTid.x -> NdotV (0.0 ~ 1.0)
    // DTid.y -> Roughness (0.0 ~ 1.0)
    float NdotV = (float(DTid.x) + 0.5) / float(width);
    float roughness = (float(DTid.y) + 0.5) / float(height);

    // 积分
    float2 envBRDF = IntegrateBRDF(NdotV, roughness);

    // 写入
    g_LUT[DTid.xy] = envBRDF;
}