TextureCube g_EnvironmentMap : register(t0);
SamplerState g_Sampler : register(s0); // 线性采样器

RWTexture2DArray<float4> g_IrradianceMap : register(u0);




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
    float2 uv = float2(DTid.xy) / float2(width, height);

    // 3. 根据不同的面 (Face Index) 给不同的颜色
    float3 debugColor = float3(0, 0, 0);

    // 这里的逻辑是：让每个面都有一种独特的“主色调”，同时保留 UV 渐变
    switch (DTid.z)
    {
        case 0: // +X (右) -> 红色主调
            debugColor = float3(1.0, uv.y, uv.x);
            break;
        case 1: // -X (左) -> 青色主调 (红的反色)
            debugColor = float3(0.0, uv.y, uv.x);
            break;
        case 2: // +Y (上) -> 绿色主调
            debugColor = float3(uv.x, 1.0, uv.y);
            break;
        case 3: // -Y (下) -> 洋红主调 (绿的反色)
            debugColor = float3(uv.x, 0.0, uv.y);
            break;
        case 4: // +Z (前) -> 蓝色主调
            debugColor = float3(uv.x, uv.y, 1.0);
            break;
        case 5: // -Z (后) -> 黄色主调 (蓝的反色)
            debugColor = float3(uv.x, uv.y, 0.0);
            break;
    }

    // 4. 写入结果
    g_IrradianceMap[DTid] = float4(debugColor, 1.0);
}