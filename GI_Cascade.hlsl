Texture3D<float4> voxelGrid : register(t0);
StructuredBuffer<float4> upperCascade : register(t1);
RWStructuredBuffer<float4> currCascade : register(u0);
cbuffer GICascadeCB : register(b0)
{
    int3 spatialRes;        // 当前级空间分辨率 4-2 2 3
    int probeSize;          // 当前级 probe 边长
    int3 upperSpatialRes;   // 上一级空间分辨率
    int upperProbeSize;     // 上一级 probe 边长
    int cascadeLevel;       // 0~4
    float lodFactor;        // pow(2, level) = 1, 2, 4, 8, 16
}
StructuredBuffer<float4> preFrameCascade0 : register(t2);


static int HEMI_COUNT = 6;
static float PI = 3.14159265;
//  0:-X  1:+X  2:-Y  3:+Y  4:-Z  5:+Z
static const float3 HEMI_N[6] =
{
    float3(-1, 0, 0), float3(1, 0, 0),
    float3(0, -1, 0), float3(0, 1, 0),
    float3(0, 0, -1), float3(0, 0, 1)
};
static const float3 HEMI_T[6] =
{
    float3(0, 0, 1), float3(0, 0, 1),
    float3(1, 0, 0), float3(1, 0, 0),
    float3(1, 0, 0), float3(1, 0, 0)
};
static const float3 HEMI_B[6] =
{
    float3(0, 1, 0), float3(0, 1, 0),
    float3(0, 0, 1), float3(0, 0, 1),
    float3(0, 1, 0), float3(0, 1, 0)
};

int CascadeIndex(int3 pos, int3 res, int raysPerHemi, int hemi, int ray)
{
    int voxelId = pos.x + pos.y * res.x + pos.z * res.x * res.y;
    return voxelId * (6 * raysPerHemi) + hemi * raysPerHemi + ray;
}

// ========== Ray 方向计算==========
float3 ComputeDir(int rayIndex, int probeSize, float3 N, float3 T, float3 B)
{
    float2 uv = float2(rayIndex % probeSize, rayIndex / probeSize) + 0.5;
    float3 localDir;
    
    if (probeSize < 4) // Cascade 0: probeSize = 3
    {
        float2 rel = uv - 1.5;
        if (length(rel) < 0.1)
        {
            localDir = float3(0, 0, 1);
        }
        else
        {
            float phi = atan2(rel.x, rel.y) + PI * 1.75;
            float theta = PI * 0.25;
            localDir = float3(sin(phi) * sin(theta), cos(phi) * sin(theta), cos(theta));
        }
    }
    else // Cascade 1-4: probeSize = 6, 12, 24, 48
    {
        float2 rel = uv - probeSize * 0.5;
        float thetaLevel = max(abs(rel.x), abs(rel.y));
        float theta = thetaLevel / (float)probeSize * PI;
        float phi = 0;
        
        if (rel.x + 0.5 > thetaLevel && rel.y - 0.5 > -thetaLevel)
            phi = rel.x - rel.y;
        else if (rel.y - 0.5 < -thetaLevel && rel.x - 0.5 > -thetaLevel)
            phi = thetaLevel * 2.0 - rel.y - rel.x;
        else if (rel.x - 0.5 < -thetaLevel && rel.y + 0.5 < thetaLevel)
            phi = thetaLevel * 4.0 - rel.x + rel.y;
        else
            phi = thetaLevel * 8.0 - (rel.y - rel.x);
            
        phi = phi * PI * 2.0 / (4.0 + 8.0 * floor(thetaLevel));
        localDir = float3(sin(phi) * sin(theta), cos(phi) * sin(theta), cos(theta));
    }
    
    return normalize(T * localDir.x + B * localDir.y + N * localDir.z);
}
// ========== 积分权重 ==========
//
//  每根 ray 的 radiance 要乘以它覆盖的立体角 × cosθ
//  这样 9 根 ray 直接加起来就是正确的 irradiance
//
float ComputeWeight(int rayIndex, int pSize, float3 dir, float3 N)
{
    float cosTheta = dot(dir, N);
    
    if (pSize <= 3)
    {
        // Cascade 0
        if (rayIndex == 4)
            return (1.0 - cos(PI * 0.25)) * cosTheta;
        else
            return cos(PI * 0.25) / 8.0 * cosTheta;
    }
    
    // 高级 cascade
    int rx = rayIndex % pSize;
    int ry = rayIndex / pSize;
    float2 rel = float2(rx, ry) + 0.5 - float2(pSize, pSize) * 0.5;
    float thetai = max(abs(rel.x), abs(rel.y));
    float theta = acos(cosTheta);
    float dTheta = PI * 0.5 / (float(pSize) * 0.5);
    float ringRays = 4.0 + 8.0 * floor(thetai);
    
    float solidAngle = (cos(theta - dTheta * 0.5) - cos(theta + dTheta * 0.5)) / ringRays;
    return solidAngle * cosTheta;
}

// ========== 从 Cascade0 获取上一帧/本帧的辐照度 ==========
float3 IntegrateVoxel(int3 p, float3 n)
{
    float3 an = abs(n);
    int hemi;
    if (an.x > max(an.y, an.z))
        hemi = (n.x < 0) ? 0 : 1;
    else if (an.y > an.z)
        hemi = (n.y < 0) ? 2 : 3;
    else
        hemi = (n.z < 0) ? 4 : 5;
        
    float3 radiance = float3(0, 0, 0);
    // 这里如果直接复用本帧刚开始写的 cascade0 会有问题（只能做到同一帧内的部分 Bleeding），
    // 原版 Shadertoy 是每帧交替读取上一帧的 Cascade0 纹理池。
    // 但是我们可以用来做自发光散发！如果你后续加入多帧 feedback 则这里读上一帧。
    // 我们目前如果没有上一级的 Texture 就先给一个简单的微光作为底色
    return radiance;
}

float4 TraceRay(float3 origin, float3 dir)
{
    float3 invDir = 1.0 / dir;
    float3 t1 = (0.01 - origin) * invDir;
    float3 t2 = (float3(31.99, 31.99, 47.99) - origin) * invDir;
    
    float3 tMin = min(t1, t2);
    float3 tMax = max(t1, t2);
    
    float tNear = max(max(tMin.x, tMin.y), tMin.z);
    float tFar = min(min(tMax.x, tMax.y), tMax.z);
    
    float t = max(tNear, 0.001);
    if (tFar < t)
        return float4(0, 0, 0, -1);
    
    int3 cell = int3(floor(origin + dir * t));
    int3 step = int3(sign(dir));
    float3 tDelta = abs(invDir);
    float3 tNext = (float3(cell) + saturate(float3(step)) - origin) * invDir;
    int3 hitNormalSign = int3(0, 0, 0);

    for (int i = 0; i < 128; i++)
    {
        if (t > tFar)
            break;
        if (any(cell < 0) || cell.x >= 32 ||
            cell.y >= 32 || cell.z >= 48)
            break;
        
        float4 voxel = voxelGrid.Load(int4(cell, 0));
        
        if (voxel.w > 0.5)
        {
            // 命中自发光（w=2）→ 返回发光色
            if (voxel.w > 1.5)
                return float4(voxel.xyz, t);
            return float4(0, 0, 0, t);

        }
        
        // DDA 步进
        hitNormalSign = int3(0,0,0);
        if (tNext.x < tNext.y && tNext.x < tNext.z)
        {
            t = tNext.x;
            tNext.x += tDelta.x;
            hitNormalSign.x = -step.x;
            cell.x += step.x;
        }
        else if (tNext.y < tNext.z)
        {
            t = tNext.y;
            tNext.y += tDelta.y;
            hitNormalSign.y = -step.y;
            cell.y += step.y;
        }
        else
        {
            t = tNext.z;
            tNext.z += tDelta.z;
            hitNormalSign.z = -step.z;
            cell.z += step.z;
        }
    }
    float sky = pow(max(dir.y, 0.0) * 0.5 + 0.5, 2.0) * 5.0;
    return float4(sky * float3(0.75, 0.85, 1.0), 100000.0);
}

// ========== Probe 位置偏移（避免嵌入几何体）==========
bool IsOutsideGeo(int3 p)
{
    if (any(p < 0) || p.x >= 32 || p.y >= 32 || p.z >= 48)
        return true;
    return voxelGrid.Load(int4(p, 0)).w < 0.5;
}

float3 GeoOffset(float3 pos)
{
    for (int x = -1; x <= 0; x++)
        for (int y = -1; y <= 0; y++)
            for (int z = -1; z <= 0; z++)
            {
                if (IsOutsideGeo(int3(floor(pos)) + int3(x, y, z)))
                    return float3(x, y, z) * 0.5;
            }
    return float3(0.001, 0.001, 0.001);
}

// ========== 上级 Cascade 的方向反查 ==========
float2 ProjectDir(float3 dir, int pSize)
{
    if (dir.z <= 0.0)
        return float2(-1, -1);
    float thetai = min(
        floor((1.0 - acos(length(dir.xy) / length(dir)) / (PI * 0.5)) * (float(pSize) * 0.5)),
        float(pSize) * 0.5 - 1.0
    );
    float phiF = atan2(-dir.x, -dir.y);
    float phiI = floor((phiF / PI * 0.5 + 0.5) * (4.0 + 8.0 * thetai) + 0.5) + 0.5;
    
    float2 phiUV;
    float phiLen = 2.0 * thetai + 1.0;
    float sideLen = phiLen + 1.0;
    
    if (phiI < phiLen) 
        phiUV = float2(sideLen - 0.5, sideLen - phiI);
    else if (phiI < phiLen * 2.0) 
        phiUV = float2(sideLen - (phiI - phiLen), 0.5);
    else if (phiI < phiLen * 3.0) 
        phiUV = float2(0.5, phiI - phiLen * 2.0);
    else 
        phiUV = float2(phiI - phiLen * 3.0, sideLen - 0.5);
        
    float offset = ((float)pSize - sideLen) * 0.5;
    return float2(offset, offset) + phiUV;
}

// ========== Merge：从上级 cascade 采样 ==========
float4 SampleUpperCascade(float3 worldPos, int rayIndex, int hemi)
{
    // 当前体素在上级的连续坐标
    float3 upperPos = worldPos / (lodFactor * 2.0);
    upperPos = clamp(upperPos, float3(0.5, 0.5, 0.5), float3(upperSpatialRes) - 0.5);
    
    // 8 个最近的上级 probe（三线性插值）
    int3 base = int3(floor(upperPos - 0.5));
    base = clamp(base, int3(0, 0, 0), upperSpatialRes - 2);
    float3 frac = min(float3(1, 1, 1), upperPos - 0.5 - float3(base));
    
    int rx = rayIndex % probeSize;
    int ry = rayIndex / probeSize;
    int urx = rx * 2;
    int ury = ry * 2;
    int upperRaysPerHemi = upperProbeSize * upperProbeSize;

        // 当前 ray 在上级对应一个 2x2 的子 ray 块
    int r00 = urx + ury * upperProbeSize;
    int r10 = urx + 1 + ury * upperProbeSize;
    int r01 = urx + (ury + 1) * upperProbeSize;
    int r11 = urx + 1 + (ury + 1) * upperProbeSize;
    
    float3 total = float3(0, 0, 0);
    
    for (int dz = 0; dz <= 1; dz++)
        for (int dy = 0; dy <= 1; dy++)
            for (int dx = 0; dx <= 1; dx++)
            {
                int3 probePos = base + int3(dx, dy, dz);
        
                // 三线性权重
                float3 w3 = lerp(1.0 - frac, frac, float3(dx, dy, dz));
                float w = w3.x * w3.y * w3.z;
        
                int baseIdx = CascadeIndex(probePos, upperSpatialRes, upperRaysPerHemi, hemi, 0);
                
                float3 avg4 = (upperCascade[baseIdx + r00].xyz + 
                               upperCascade[baseIdx + r10].xyz + 
                               upperCascade[baseIdx + r01].xyz + 
                               upperCascade[baseIdx + r11].xyz) * 0.25;
        
                total += avg4 * w;
            }
    
    return float4(total, 1.0);
}
// [numthreads(64, 1, 1)]
// void CSMain(uint3 dtid : SV_DispatchThreadID)
// {
//     // 只让第一个线程输出调试信息，其他直接返回
//     if (dtid.x > 0 || dtid.y > 0) return;
    
//     // 把常量写到 buffer 的前几个位置
//     currCascade[0] = float4(spatialRes, probeSize);
//     currCascade[1] = float4(upperSpatialRes, upperProbeSize);
//     currCascade[2] = float4(cascadeLevel, lodFactor, probeSize * probeSize, 0);
//     return;
// }

[numthreads(64, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    int totalXY = spatialRes.x * spatialRes.y;
    
    int3 voxelPos;
    // dtid.x -> pos.xy，dtid.y -> pos.z
    voxelPos.x = dtid.x % spatialRes.x;
    voxelPos.y = dtid.x / spatialRes.x;
    voxelPos.z = dtid.y;
    
    if(dtid.x >= totalXY || voxelPos.z >= spatialRes.z)
        return;
    
    float3 voxelWorldPos = (float3(voxelPos) + 0.5) * lodFactor;
    
    float4 currGridInfo = voxelGrid.Load(int4(floor(voxelWorldPos), 0));
    
    
    if (currGridInfo.w > 0.5)
    {
        // in the block ?
        int raysPerHemi = probeSize * probeSize;
        for(int hemi = 0; hemi < HEMI_COUNT; ++hemi)
        {
            for (int ray = 0; ray < raysPerHemi; ++ray)
            {
                int index = CascadeIndex(voxelPos, spatialRes, raysPerHemi, hemi, ray);
                currCascade[index] = float4(0, 0, 0, 0);
            }
        }
        return;
    };
    // how many rays per hemi?
    int raysPerHemi = probeSize * probeSize;

    // cascade 0 的线程里，临时调试
    // if (cascadeLevel == 0)
    // {
    //     // 不用 SampleUpperCascade，手动读一个上级 probe 全部 ray 求和
    //         int upperRPH = upperProbeSize * upperProbeSize;
    //         int3 upperProbe = int3(floor(voxelWorldPos / 2.0));
    //         upperProbe = clamp(upperProbe, int3(0, 0, 0), upperSpatialRes - 1);
    
    //         // 根据当前体素法线选半球（而不是固定 hemi=3）
    //         // 临时用 hemi=0 到 5 都试一下
    //         float3 bestSum = float3(0, 0, 0);
    //         for (int h = 0; h < 6; h++)
    //         {
    //             float3 sum = float3(0, 0, 0);
    //             for (int r = 0; r < upperRPH; r++)
    //                 sum += upperCascade[CascadeIndex(upperProbe, upperSpatialRes, upperRPH, h, r)].xyz;
    //             bestSum = max(bestSum, sum);
    //         }
    
    //     // 不放大，看原始值
    //         currCascade[CascadeIndex(voxelPos, spatialRes, raysPerHemi, 0, 0)] = float4(bestSum, 1.0);
    //     // 同时写到所有半球让所有表面都能看到
    //         for (int h = 1; h < 6; h++)
    //             currCascade[CascadeIndex(voxelPos, spatialRes, raysPerHemi, h, 0)] = float4(bestSum, 1.0);
    //         return;
    // }

    // 正常的 hemi/ray 循环在下面...
    for (int hemi = 0; hemi < HEMI_COUNT; ++hemi)
    {
        float3 N = HEMI_N[hemi];
        float3 T = HEMI_T[hemi];
        float3 B = HEMI_B[hemi];

        // how to get ray dir from hemi index and ray index?
        float3 probePos;
        if(cascadeLevel == 0)
        {
            probePos = voxelWorldPos - N * 0.25;
        }
        else
        {
            probePos = voxelWorldPos + GeoOffset(voxelWorldPos);  
        }
        for (int ray = 0; ray < raysPerHemi; ++ray)
        {
            float3 dir = ComputeDir(ray, probeSize, N, T, B);
            float4 rayResult = TraceRay(probePos, dir);
            
            float weight = ComputeWeight(ray, probeSize, dir, N);
            rayResult.xyz = rayResult.xyz * weight;
            
            if (cascadeLevel < 4)         // 应该通过 CBuffer 传最大级别
            {
                float4 upperLight = SampleUpperCascade(voxelWorldPos, ray, hemi);
                float distInterp = clamp((rayResult.w - lodFactor) / lodFactor * 0.5, 0.0, 1.0);
                rayResult.xyz = lerp(rayResult.xyz, upperLight.xyz, distInterp);
                rayResult.xyz *= 2.0; // 适当放大亮度，抵消插值带来的衰减（现在 Cascade 存储的是纯 Radiance 而不是 Irradiance 了）
                // rayResult.xyz = upperLight.xyz;
            }

            currCascade[CascadeIndex(voxelPos, spatialRes, raysPerHemi, hemi, ray)]
                = rayResult;
        }
    }
}