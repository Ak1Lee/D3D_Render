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
static const float3 SUN_DIR = normalize(float3(0.5, 1.0, -0.3));
static const float3 SUN_COLOR = float3(2.0, 1.5, 1.25);

bool IsShadowed(float3 origin)
{
    float3 dir = SUN_DIR;
    float3 invDir;
    invDir.x = (abs(dir.x) < 1e-8) ? 1e10 : 1.0 / dir.x;
    invDir.y = (abs(dir.y) < 1e-8) ? 1e10 : 1.0 / dir.y;
    invDir.z = (abs(dir.z) < 1e-8) ? 1e10 : 1.0 / dir.z;

    float3 t1 = (0.01 - origin) * invDir;
    float3 t2 = (float3(31.99, 31.99, 47.99) - origin) * invDir;
    float3 tMin = min(t1, t2);
    float3 tMax = max(t1, t2);
    float tNear = max(max(tMin.x, tMin.y), tMin.z);
    float tFar  = min(min(tMax.x, tMax.y), tMax.z);
    float t = max(tNear, 0.001);
    if (tFar < t) return false;

    int3 cell = int3(floor(origin + dir * t));
    int3 step = int3(sign(dir));
    float3 tDelta = abs(invDir);
    float3 tNext = (float3(cell) + saturate(float3(step)) - origin) * invDir;
    if (step.x == 0) tNext.x = 1e10;
    if (step.y == 0) tNext.y = 1e10;
    if (step.z == 0) tNext.z = 1e10;

    for (int i = 0; i < 64; i++)
    {
        if (t > tFar) return false;
        if (any(cell < 0) || cell.x >= 32 || cell.y >= 32 || cell.z >= 48) return false;

        float4 voxel = voxelGrid.Load(int4(cell, 0));
        if (voxel.w > 0.5 && voxel.w < 1.5) return true;  // solid → shadow
        if (voxel.w > 1.5) return false;                   // emissive → lit

        if (tNext.x < tNext.y && tNext.x < tNext.z) { t = tNext.x; tNext.x += tDelta.x; cell.x += step.x; }
        else if (tNext.y < tNext.z)              { t = tNext.y; tNext.y += tDelta.y; cell.y += step.y; }
        else                                     { t = tNext.z; tNext.z += tDelta.z; cell.z += step.z; }
    }
    return false;
}

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

#define TRILINEAR_INTEGRATE 0  // 1=三线性插值采样上一帧Cascade0  0=单点采样

// 周围圆环 (45度 < Theta < 90度) 的完整积分也为 PI / 2
// 我们提前将公式中的除以 PI (反照率 Albedo/PI 的 Lambert 公式) 塞了进去：
static const float CASC0_WEIGHT[9] = {
    0.0625, 0.0625, 0.0625,
    0.0625, 0.5,    0.0625,
    0.0625, 0.0625, 0.0625
};

// ========== 从 Cascade0 获取上一帧/本帧的辐照度 ==========
float3 IntegrateVoxel(int3 vp, float3 n)
{
    if (any(vp < 0) || vp.x >= 32 || vp.y >= 32 || vp.z >= 48)
        return float3(0, 0, 0);

    float3 an = abs(n);
    int hemi;
    if (an.x > max(an.y, an.z))
        hemi = (n.x < 0) ? 0 : 1;
    else if (an.y > an.z)
        hemi = (n.y < 0) ? 2 : 3;
    else
        hemi = (n.z < 0) ? 4 : 5;
        
    float3 radiance = float3(0, 0, 0);
    int3 res = int3(32, 32, 48);
    for (int i = 0; i < 9; i++)
    {
        radiance += preFrameCascade0[CascadeIndex(vp, res, 9, hemi, i)].xyz * CASC0_WEIGHT[i];
    }

    return radiance;
}

float3 IntegrateVoxelTrilinear(float3 worldPos, float3 normal)
{
    float3 probeCoord = worldPos - 0.5;
    int3 base = int3(floor(probeCoord));
    base = clamp(base, int3(0, 0, 0), int3(31, 31, 47));
    float3 frac = clamp(probeCoord - float3(base), 0.0, 1.0);

    float3 an = abs(normal);
    int hemi;
    if (an.x > max(an.y, an.z))
        hemi = (normal.x < 0) ? 0 : 1;
    else if (an.y > an.z)
        hemi = (normal.y < 0) ? 2 : 3;
    else
        hemi = (normal.z < 0) ? 4 : 5;

    float3 result = float3(0, 0, 0);
    float weightSum = 0;
    int3 res = int3(32, 32, 48);
    for (int dz = 0; dz <= 1; dz++)
        for (int dy = 0; dy <= 1; dy++)
            for (int dx = 0; dx <= 1; dx++)
            {
                int3 probePos = base + int3(dx, dy, dz);
                if (any(probePos < 0) || probePos.x >= 32 || probePos.y >= 32 || probePos.z >= 48)
                    continue;
                if (voxelGrid.Load(int4(probePos, 0)).w > 0.5)
                    continue;

                float3 s = smoothstep(0.0, 1.0, frac);
                float3 w3 = lerp(1.0 - s, s, float3(dx, dy, dz));
                float w = w3.x * w3.y * w3.z;

                float3 radiance = float3(0, 0, 0);
                for (int i = 0; i < 9; i++)
                    radiance += preFrameCascade0[CascadeIndex(probePos, res, 9, hemi, i)].xyz * CASC0_WEIGHT[i];

                result += radiance * w;
                weightSum += w;
            }
    return weightSum > 0 ? result / weightSum : result;
}

float4 TraceRay(float3 origin, float3 dir)
{
    // 安全倒数：防止 dir 分量为 0 产生 INF/NaN
    // 注意：sign(0)=0 在 HLSL 中！所以用 1e10 而非 sign()*1e10
    float3 invDir;
    invDir.x = (abs(dir.x) < 1e-8) ? 1e10 : 1.0 / dir.x;
    invDir.y = (abs(dir.y) < 1e-8) ? 1e10 : 1.0 / dir.y;
    invDir.z = (abs(dir.z) < 1e-8) ? 1e10 : 1.0 / dir.z;

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
    // 方向为 0 的轴永远不该被选中 → 设巨大值
    if (step.x == 0) tNext.x = 1e10;
    if (step.y == 0) tNext.y = 1e10;
    if (step.z == 0) tNext.z = 1e10;
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
                return float4(voxel.xyz * 5.0, t);

            // 命中普通固体（w=1）
            float3 hitNorm = float3(hitNormalSign);
            hitNorm = normalize(hitNorm);
            float3 hitPos = origin + dir * t;

#if TRILINEAR_INTEGRATE
            float3 indirect = IntegrateVoxelTrilinear(hitPos + hitNorm * 0.5, hitNorm);
#else
            int3 probePosForPrevFrame = cell + hitNormalSign;
            float3 indirect = IntegrateVoxel(probePosForPrevFrame, hitNorm);
#endif
            float NdotL = max(dot(hitNorm, SUN_DIR), 0);
            float shadow = 1.0;
            if (NdotL > 0)
                shadow = IsShadowed(hitPos + hitNorm * 0.5) ? 0.0 : 1.0;
            return float4(voxel.xyz * (indirect + SUN_COLOR * NdotL * shadow), t);
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
    float sky = pow(max(dir.y, 0.0) * 0.5 + 0.5, 2.0) * 1.0;
    return float4(sky * float3(0.75, 0.85, 1.0), 100000.0);
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

    // ===== 调试：正常跑 ray tracing，然后 dump 左右探针的 TraceRay 原始结果 =====
    // 正常的 hemi/ray 循环在下面...
    for (int hemi = 0; hemi < HEMI_COUNT; ++hemi)
    {
        float3 N = HEMI_N[hemi];
        float3 T = HEMI_T[hemi];
        float3 B = HEMI_B[hemi];

        float3 probePos = voxelWorldPos - N * 0.25;
        for (int ray = 0; ray < raysPerHemi; ++ray)
        {
            float3 dir = ComputeDir(ray, probeSize, N, T, B);
            float4 rayResult = TraceRay(probePos, dir);

            float weight = ComputeWeight(ray, probeSize, dir, N);
            // rayResult.xyz = rayResult.xyz * weight;  // 存储时不加权，读取时用 CASC0_WEIGHT 积分

            if (cascadeLevel < 4)
            {
                float4 upperLight = SampleUpperCascade(voxelWorldPos, ray, hemi);
                float distInterp = clamp((rayResult.w - lodFactor) / lodFactor * 0.5, 0.0, 1.0);
                rayResult.xyz = lerp(rayResult.xyz, upperLight.xyz, distInterp) * 1.0;
            }

            currCascade[CascadeIndex(voxelPos, spatialRes, raysPerHemi, hemi, ray)]
                = rayResult;
        }
    }
}