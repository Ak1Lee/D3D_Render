RWTexture3D<float4> voxelGrid : register(u0);

// 0 = Cornell Box, 1 = RCSample 参考场景
#define SCENE_TYPE 1

float DFBox(float3 p, float3 b)
{
    float3 d = abs(p - b * 0.5) - b * 0.5;
    return min(max(d.x, max(d.y, d.z)), 0.) + length(max(d, 0.));
}

float smin(float a, float b, float k)
{
    float h = max(k - abs(a - b), 0.) / k;
    return min(a, b) - h * h * h * k * (1.0 / 6.0);
}

float4 Scene_CornellBox(float3 vPos)
{
    float4 info = float4(0, 0, 0, 0);

    // 地板 (白)
    if (vPos.y < 1.0)
        info = float4(0.9, 0.9, 0.9, 1.0);

    // 天花板 (白)
    if (vPos.y > 30.0)
        info = float4(0.9, 0.9, 0.9, 1.0);

    // 后墙 (白)
    if (vPos.z > 28.0)
        info = float4(0.9, 0.9, 0.9, 1.0);

    // 左墙 (红)
    if (vPos.x < 1.0)
        info = float4(0.9, 0.2, 0.2, 1.0);

    // 右墙 (绿)
    if (vPos.x > 30.0)
        info = float4(0.2, 0.9, 0.2, 1.0);

    // 天花板灯 (自发光)
    if (vPos.y > 29.0 && vPos.x >= 13.0 && vPos.x <= 18.0
                && vPos.z >= 13.0 && vPos.z <= 18.0)
        info = float4(10.0, 10.8, 10.5, 2.0);

    // 小方块 (白)
    if (vPos.x > 5.0 && vPos.x < 12.0
     && vPos.y > 5.0 && vPos.y < 10.0
     && vPos.z > 8.0 && vPos.z < 15.0)
        info = float4(0.9, 0.9, 0.9, 1.0);

    // 高方块 (白)
    if (vPos.x > 18.0 && vPos.x < 25.0
     && vPos.y > 0.0  && vPos.y < 20.0
     && vPos.z > 16.0 && vPos.z < 23.0)
        info = float4(0.9, 0.9, 0.9, 1.0);

    return info;
}

float4 Scene_RCSample(float3 vPos)
{
    float4 info = float4(0, 0, 0, 0);
    float3 stoneColor = float3(0.95, 0.925, 0.9);

    // ===== 房间边界 =====
    if (DFBox(vPos - float3(1, 1, 1), float3(30, 100, 46)) > 0)
        info = float4(stoneColor, 1);

    // ===== 地板棋盘格 =====
    if (vPos.y < 2
        && frac(dot(vPos.xz, float2(0.707, 0.707)) * 0.125) > 0.35
        && frac(dot(vPos.xz, float2(0.707, -0.707)) * 0.125) > 0.35)
        info = float4(stoneColor, 1);

    // ===== 移动自发光（固定初始位置）=====
    if (info.w > 0.5 && abs(vPos.y - 1.5) < 0.5
        && length(vPos.xz - float2(8, 8)) < 5)
        info = float4(1.5, 0.7, 0.2, 2);

    // ===== 柱子 =====
    if (length(float2(vPos.x - 16, fmod(vPos.z + 8, 16) - 8)) < 2)
        info = float4(stoneColor, 1);

    // ===== 拱门 / 二楼结构 =====
    float3 modP = float3(vPos.xy, fmod(vPos.z, 16));
    if (vPos.y > 19) modP.y -= 16;
    if (DFBox(float3(vPos.x, modP.y, vPos.z) - float3(14, 9, 0), float3(3, 10, 32)) < 0
        && length(modP.zy - float2(8, 8)) - abs(vPos.x - 15.5) > 6)
        info = float4(stoneColor, 1);
    if (DFBox(float3(vPos.x, modP.y, vPos.z) - float3(16, 9, 30), float3(16, 10, 3)) < 0
        && length(modP.xy - float2(24, 8)) - abs(vPos.z - 31.5) > 6)
        info = float4(stoneColor, 1);

    // ===== 红绿布料（加粗版，适合 32³ 分辨率）=====
    if (vPos.z < 32)
    {
        // 红布 z≈8，x≈18，y≈3-16
        float3 aPosR = float3(abs(vPos.x - 18), vPos.y, abs(vPos.z - 8));
        float distR = 1.0 + (17.0 - vPos.y) * (17.0 - vPos.y) * 0.02;
        bool inBoxR = DFBox(vPos - float3(17, 3, 1), float3(2, 13, 14)) < 0;
        if (inBoxR && aPosR.z > distR && abs(vPos.x - 18.0) < 1.5)
            info = float4(0.99, 0.4, 0.4, 1);

        // 绿布 z≈24
        float3 aPosG = float3(abs(vPos.x - 18), vPos.y, abs(vPos.z - 24));
        float distG = 1.0 + (17.0 - vPos.y) * (17.0 - vPos.y) * 0.02;
        bool inBoxG = DFBox(vPos - float3(17, 3, 17), float3(2, 13, 14)) < 0;
        if (inBoxG && aPosG.z > distG && abs(vPos.x - 18.0) < 1.5)
            info = float4(0.4, 0.99, 0.4, 1);
    }

    // ===== 二楼地板 =====
    if (DFBox(vPos - float3(0, 15, 0), float3(16, 1, 48)) < 0)
        info = float4(stoneColor, 1);
    if (DFBox(vPos - float3(0, 15, 32), float3(32, 1, 16)) < 0)
        info = float4(stoneColor, 1);

    // 内拱
    if (vPos.x < 16
        && fmod(vPos.y, 16) > 15 - pow(0.22 * length(float2(vPos.x - 8, modP.z - 8)), 2)
        && length(modP - float3(12, 5, 8)) > 10)
        info = float4(stoneColor, 1);
    if (vPos.x > 16 && vPos.z > 32
        && fmod(vPos.y, 16) > 15 - pow(0.22 * length(float2(vPos.x - 24, modP.z - 8)), 2)
        && length(modP - float3(24, 5, 4)) > 10)
        info = float4(stoneColor, 1);

    // 喷泉
    if (vPos.y < 7
        && length(vPos.xz - float2(8, 40)) < 2 + floor((vPos.y - 1) * 0.5)
        && vPos.y < 3 + length(vPos.xz - float2(8, 40)))
        info = float4(stoneColor, 1);

    // 喷泉灯具
    if (length(float2(length(vPos.xz - float2(8, 40))
        - 3.5 - floor((13.5 - vPos.y) * 0.333), vPos.y - 13.5)) < 0.5)
        info = float4(0.8, 0.6, 0.2, 2);

    // ===== X+ 墙砖块 =====
    if (vPos.x < 2
        && fmod(vPos.z + floor((vPos.y + 1) / 4) * 2, 4) > 1
        && fmod(vPos.y - 1, 4) > 2)
        info = float4(stoneColor, 1);

    // ===== Z- 墙狮子（简化）=====
    if (smin(length(vPos - float3(24, 8, 51)) - 6,
             length(vPos - float3(24, 7, 45.5)) - 2, 4) < 0
        && length(float3(abs(vPos.x - 24) - 2, vPos.y - 9, vPos.z - 44)) > 1)
        info = float4(stoneColor, 1);

    // ===== Z+ 墙 =====
    if (vPos.x > 16)
    {
        if (length(float3(fmod(vPos.x + 1, 8) - 4, fmod(vPos.y, 8) - 4, vPos.z - 0.15)) < 2.5)
            info = float4(stoneColor, 1);
    }

    // ===== 天花板 =====
    if (DFBox(vPos - float3(0, 30, 0), float3(16, 2, 48)) < 0)
        info = float4(stoneColor, 1);
    if (DFBox(vPos - float3(0, 30, 32), float3(32, 2, 16)) < 0)
        info = float4(stoneColor, 1);

    // ===== 动态球体（固定位置）=====
    if (length(vPos - float3(27, 24, 8)) < 4)
        info = float4(0.5, 0.6, 0.9, 1);

    return info;
}

[numthreads(8, 8, 8)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float3 vPos = float3(id) + 0.5;
    float4 info;

#if SCENE_TYPE == 0
    info = Scene_CornellBox(vPos);
#else
    info = Scene_RCSample(vPos);
#endif

    voxelGrid[id] = info;
}
