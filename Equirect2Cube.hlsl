// Equirect2Cube.hlsl

// 输入：t0 (那张 HDR 全景图)
Texture2D g_EquirectangularMap : register(t0);
SamplerState g_Sampler : register(s0); // 线性采样器

// 输出：u0 (立方体贴图，RWTexture2DArray 代表它是可写的数组)
// Cubemap 其实就是 6 张 Layer 的 2D 纹理数组
RWTexture2DArray<float4> g_OutputCubemap : register(u0);

static const float2 invAtan = float2(0.1591, 0.3183);

// 辅助函数：根据 3D 方向算出 2D UV
float2 SampleSphericalMap(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    uv.y = 1.0 - uv.y;
    return uv;
}

// 辅助函数：根据面索引 (0~5) 和 UV，反推 3D 方向
float3 GetDirFromCubeFace(uint faceIdx, float2 uv)
{
    float u = uv.x * 2.0 - 1.0;
    float v = uv.y * 2.0 - 1.0;
    // DX12 左手系下的 Cubemap 面朝向
    switch (faceIdx)
    {
        case 0:
            return float3(1.0, v, -u); // +X
        case 1:
            return float3(-1.0, v, u); // -X
        case 2:
            return float3(u, 1.0, -v); // +Y
        case 3:
            return float3(u, -1.0, v); // -Y
        case 4:
            return float3(u, v, 1.0); // +Z
        case 5:
            return float3(-u, v, -1.0); // -Z
    }
    return float3(0, 0, 0);
}

// 线程组定义：每组处理 32x32 个像素，Z=1
[numthreads(32, 32, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    // DTid.x, y 是像素坐标
    // DTid.z 是切片索引 (也就是 Cubemap 的第几个面，0~5)
    
    uint width, height, elements;
    g_OutputCubemap.GetDimensions(width, height, elements);

    if (DTid.x >= width || DTid.y >= height)
        return;

    // 1. 归一化 UV [0,1]
    float2 uv = float2((DTid.x + 0.5) / width, (DTid.y + 0.5) / height);
    
    // 注意：Compute Shader 写图通常是倒着的，需要 Flip Y
    // 如果发现天空盒上下颠倒，把下面这行解开
    uv.y = 1.0 - uv.y; 

    // 2. 算出 3D 向量
    float3 dir = normalize(GetDirFromCubeFace(DTid.z, uv));

    // 3. 采样 HDR 图
    float3 color = g_EquirectangularMap.SampleLevel(g_Sampler, SampleSphericalMap(dir), 0).rgb;

    // 4. 写入结果
    g_OutputCubemap[int3(DTid.x, DTid.y, DTid.z)] = float4(color, 1.0);
}