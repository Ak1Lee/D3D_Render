TextureCube g_EnvironmentMap : register(t0);
SamplerState g_Sampler : register(s0); // 线性采样器

RWTexture2DArray<float4> g_IrradianceMap : register(u0);


static const float PI = 3.14159265;
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

float3 CosineSampleHemisphere(float2 Xi)
{

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt(1.0 - Xi.y);
    float sinTheta = sqrt(Xi.y);

    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    
    return H;
}



// 线程组定义：每组处理 32x32 个像素，Z=1
[numthreads(32, 32, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    // DTid.x, DTid.y = 像素坐标 (0~31)
    // DTid.z = 面索引 (0~5)

    // 1. 获取尺寸 (32x32)
    uint width, height, elements;
    g_IrradianceMap.GetDimensions(width, height, elements);

    // 2. 计算 UV (0.0 ~ 1.0)
    float2 uv = (float2(DTid.xy)+0.5f) / float2(width, height);
    uv = uv * 2.0 - 1.0;
    uv.y = -uv.y; // 翻转 Y 轴
    float3 dir;
    int faceIdx = DTid.z;
    
    // DX12 CubeMap 的标准面定义：
    // 0: +X, 1: -X
    // 2: +Y, 3: -Y
    // 4: +Z, 5: -Z
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
    float3 N = normalize(dir);
    
    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, N));
    if (length(cross(up, N)) < 0.01)
    { // N 和 up 平行时叉积为零
        up = float3(1.0, 0.0, 0.0); 
        right = normalize(cross(up, N));
    }
    up = normalize(cross(N, right));
    float3 irradiance = float3(0.0, 0.0, 0.0);

    const int SAMPLE_COUNT = 4096;
    for (int i = 0; i < SAMPLE_COUNT; i++)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 L_tangent = CosineSampleHemisphere(Xi);
        float3 L_world = L_tangent.x * right + L_tangent.y * up + L_tangent.z * N;
        float3 envColor = g_EnvironmentMap.SampleLevel(g_Sampler, L_world, 3.5).rgb;
        float maxBrightness = 20.0f;
        envColor = min(envColor, maxBrightness);
        
        irradiance += envColor;
    }

    // float sampleDelta = 0.25f;
    // float sampleCount = 0.0f;
    // for (float phi = 0.0f; phi < 2.0f * 3.14159265f; phi += sampleDelta)
    // {
    //     for (float theta = 0.0f; theta < 0.5f * 3.14159265f; theta += sampleDelta)
    //     {
    //         float3 SampleDir = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
    //         SampleDir = SampleDir.x * right + SampleDir.y * up + SampleDir.z * N;
            
    //         float3 envColor = g_EnvironmentMap.SampleLevel(g_Sampler, SampleDir, 0).rgb;
    //         irradiance += envColor * cos(theta) * sin(theta);
    //         sampleCount++;

    //     }

    // }
    // irradiance = irradiance / float(sampleCount);
    // irradiance = irradiance * (3.14159265f / float(sampleCount));

    irradiance = irradiance / float(SAMPLE_COUNT);
    
    
    //float3 color = g_EnvironmentMap.SampleLevel(g_Sampler, N, 0).rgb;
    g_IrradianceMap[DTid] = float4(irradiance, 1.0);
    // 4. 写入结果
    // g_IrradianceMap[DTid] = float4(debugColor, 1.0);
}