Texture3D<float4> voxelGrid : register(t0);
RWStructuredBuffer<float4> cascade0 : register(u0);

static const int3 VOXEL_RES = int3(32, 32, 48);
static const int HEMI_COUNT = 6;
static const int RAY_COUNT = 9; // 3×3
static const int RAYS_PER_VOXEL = HEMI_COUNT * RAY_COUNT; // 54
static const float PI = 3.14159265;


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

int CascadeIndex(int3 pos, int hemi, int ray)
{
    int voxelId = pos.x + pos.y * VOXEL_RES.x + pos.z * VOXEL_RES.x * VOXEL_RES.y;
    return voxelId * RAYS_PER_VOXEL + hemi * RAY_COUNT + ray;
}

// ========== Ray 方向计算（3×3 probe，cascade 0）==========
//
//  ray index 布局:
//    6 7 8
//    3 4 5     4 = 中心（正上方）
//    0 1 2     其余 8 个分布在 theta=π/4 的锥面上
//
float3 ComputeDir(int rayIndex, float3 N, float3 T, float3 B)
{
    int rx = rayIndex % 3 - 1; // -1, 0, 1
    int ry = rayIndex / 3 - 1; // -1, 0, 1
    
    // 中心 ray → 半球顶部方向
    if (rx == 0 && ry == 0)
        return N;
    
    // 外圈 → theta=π/4，方位角均匀分布
    float phi = atan2(float(rx), float(ry));
    float theta = PI * 0.25;
    float3 localDir = float3(sin(phi) * sin(theta),
                              cos(phi) * sin(theta),
                              cos(theta));
    return normalize(T * localDir.x + B * localDir.y + N * localDir.z);
}

// ========== 积分权重 ==========
//
//  每根 ray 的 radiance 要乘以它覆盖的立体角 × cosθ
//  这样 9 根 ray 直接加起来就是正确的 irradiance
//
float ComputeWeight(int rayIndex, float cosTheta)
{
    if (rayIndex == 4)
    {
        // 中心 ray：覆盖从顶部到 θ=π/4 的锥
        return (1.0 - cos(PI * 0.25)) * cosTheta;
    }
    else
    {
        // 外圈 8 根：平分剩余的环形带
        return cos(PI * 0.25) / 8.0 * cosTheta;
    }
}

float4 TraceRay(float3 origin, float3 dir)
{
    float3 invDir = 1.0 / dir;
    float3 t1 = (0.01 - origin) * invDir;
    float3 t2 = (float3(VOXEL_RES) - 0.01 - origin) * invDir;
    
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
    float3 tNext = (float3(cell) + step * 0.5 - origin) * invDir;
    
    for (int i = 0; i < 128; i++)
    {
        if (t > tFar)
            break;
        if (any(cell < 0) || cell.x >= VOXEL_RES.x ||
            cell.y >= VOXEL_RES.y || cell.z >= VOXEL_RES.z)
            break;
        
        float4 voxel = voxelGrid.Load(int4(cell, 0));
        
        if (voxel.w > 0.5)
        {
            // 命中自发光（w=2）→ 返回发光色
            if (voxel.w > 1.5)
                return float4(voxel.xyz, t);
            
            // 命中普通固体（w=1）→ 暂时返回 0
            // 后面加 multibounce 时这里会读上一帧的间接光
            return float4(0, 0, 0, t);
        }
        
        // DDA 步进
        if (tNext.x < tNext.y && tNext.x < tNext.z)
        {
            t = tNext.x;
            tNext.x += tDelta.x;
            cell.x += step.x;
        }
        else if (tNext.y < tNext.z)
        {
            t = tNext.y;
            tNext.y += tDelta.y;
            cell.y += step.y;
        }
        else
        {
            t = tNext.z;
            tNext.z += tDelta.z;
            cell.z += step.z;
        }
    }
    float sky = pow(dir.y * 0.5 + 0.5, 2.0) * 1.25;
    return float4(sky * float3(0.75, 0.85, 1.0), 100000.0);
}


// ========== 主函数 ==========
//
//  每个线程处理一个体素的全部 54 根 ray（6 半球 × 9 ray）
//  Dispatch(128, 48, 1) → 128×8=1024 个线程 x方向，覆盖 32×32=1024 个 xy 组合
//                        48 个线程 y 方向，覆盖 z=0~47
//

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    int3 voxelPos = int3(dtid.x % 32, dtid.x / 32, dtid.y);
    if (voxelPos.y >= VOXEL_RES.y || voxelPos.z >= VOXEL_RES.z)
        return;
    
    float4 self = voxelGrid.Load(int4(voxelPos, 0));
    if(self.w > 0.5)
    {
        for (int h = 0; h < HEMI_COUNT; h++)
        {
            for (int r = 0; r < RAY_COUNT; r++)
            {
                cascade0[CascadeIndex(voxelPos, h, r)] = float4(0, 0, 0, -1);
            }
        }
        return;

    }
    
    float3 center = (float3(voxelPos) + 0.5);
    for (int hemi = 0; hemi < HEMI_COUNT; hemi++)
    {
        
        float3 N = HEMI_N[hemi];
        float3 T = HEMI_T[hemi];
        float3 B = HEMI_B[hemi];
        float3 probePos = center - N * 0.25;
        
        for (int ray = 0; ray < RAY_COUNT; ++ray)
        {
            float3 dir = ComputeDir(ray, N, T, B);
            float4 result = TraceRay(probePos, dir);
            
            
            float weight = ComputeWeight(ray, dot(dir, N));
            result.xyz *= weight;
            cascade0[CascadeIndex(voxelPos, hemi, ray)] = result;
        }
    }
    

    
    
}