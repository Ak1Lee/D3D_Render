Texture3D<float4> voxelGrid : register(t0);
StructuredBuffer<float4> cascade0 : register(t1);

// 与 C++ 端 LightConstants 布局一致（复用 LightConstantBuffer）
cbuffer LightCB : register(b0)
{
    float3 LightDirection;
    float  LightIntensity;
    float3 LightColor;
    float  LightSize;
    float3 AmbientColor;
    float  _pad2;
    float3 cameraPos;          // = LightConstants.CameraPosition
    float  _pad3;
    float4x4 LightViewProj;
    float4x4 invViewProj;      // = LightConstants.CameraInvViewProj
};

struct VSOut { float4 Pos : SV_Position; };

// 全屏三角形：DrawInstanced(3, 1, 0, 0)
VSOut VSMain(uint vid : SV_VertexID)
{
    VSOut o;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    o.Pos = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    o.Pos.y = -o.Pos.y;
    return o;
}

int CascadeIndex(int3 pos, int hemi, int ray)
{
    int voxelId = pos.x + pos.y * 32 + pos.z * 32 * 32;
    return voxelId * 54 + hemi * 9 + ray;
}

// 对应 Cascade0 (probeSize=3) 每根 ray 的积分权重。
// 依据球面积分法则：为了得到准确出射光，积分 \int L * cos(theta) dW
// 中心区域 (Theta < 45度) 的完整积分为 PI / 2
// 周围圆环 (45度 < Theta < 90度) 的完整积分也为 PI / 2
// 我们提前将公式中的除以 PI (反照率 Albedo/PI 的 Lambert 公式) 塞了进去：
// 中心：(PI/2) / PI = 0.5
// 周围单根：(PI/2 / 8) / PI = 0.0625
// 这样加起来总和精确等于 1.0 (能量完全守恒定律)
static const float CASC0_WEIGHT[9] = {
    0.0625, 0.0625, 0.0625,
    0.0625, 0.5,    0.0625,
    0.0625, 0.0625, 0.0625
};

float3 SampleIndirect(int3 vp, float3 normal)
{
    if (any(vp < 0) || vp.x >= 32 || vp.y >= 32 || vp.z >= 48)
        return float3(0, 0, 0);
    
    float3 an = abs(normal);
    int hemi;
    if (an.x > max(an.y, an.z))
        hemi = (normal.x < 0) ? 0 : 1;
    else if (an.y > an.z)
        hemi = (normal.y < 0) ? 2 : 3;
    else
        hemi = (normal.z < 0) ? 4 : 5;
    
    float3 radiance = float3(0, 0, 0);
    for (int i = 0; i < 9; i++)
        radiance += cascade0[CascadeIndex(vp, hemi, i)].xyz * CASC0_WEIGHT[i];
    return radiance;
}

// 临时调试相机：体素空间还没和世界对齐，先用一个"住在 voxel 空间里"的虚拟相机
// 想用真实相机时把这个改成 0
#define USE_DEBUG_CAM 1

// 可视化模式：
//   0 = 完整 GI (albedo × indirect from cascade0)
//   1 = 只显示体素 albedo + 简单的法线 N·L 着色（用来确认 voxel build + DDA 是对的）
//   2 = N·L 灰度（独立检查法线方向）
#define VIZ_MODE 0

float4 PSMain(float4 pos : SV_Position) : SV_Target
{
    float2 screenSize = float2(1920.0, 1080.0); // 换成你的分辨率
    float2 uv = pos.xy / screenSize;

#if USE_DEBUG_CAM
    // 在体素 AABB [0..32, 0..32, 0..48] 外侧，沿 +Z 看进去
    const float3 dbgCamPos = float3(16.0, 16.0, -15.0);
    const float3 dbgTarget = float3(16.0, 16.0, 24.0);
    const float  dbgFovY   = 1.0;                      // ~57°
    float aspect = screenSize.x / screenSize.y;

    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float3 forward = normalize(dbgTarget - dbgCamPos);
    float3 right   = normalize(cross(float3(0, 1, 0), forward));
    float3 upAxis  = cross(forward, right);
    float  tanFov  = tan(dbgFovY * 0.5);
    float3 rayDir = normalize(forward
                            + right  * ndc.x * tanFov * aspect
                            + upAxis * ndc.y * tanFov);
    float3 cam = dbgCamPos;
#else
    float4 ndc4 = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    ndc4.y = -ndc4.y;
    float4 worldPos = mul(invViewProj, ndc4);
    float3 rayDir = normalize(worldPos.xyz / worldPos.w - cameraPos);
    float3 cam = cameraPos;
#endif

    // DDA ray march 穿过体素网格
    float3 invDir = 1.0 / rayDir;
    float3 t1 = (0.01 - cam) * invDir;
    float3 t2 = (float3(31.99, 31.99, 47.99) - cam) * invDir;
    float3 tMin = min(t1, t2);
    float3 tMax = max(t1, t2);
    float tNear = max(max(tMin.x, tMin.y), tMin.z);
    float tFar = min(min(tMax.x, tMax.y), tMax.z);
    float t = max(tNear, 0.001);

    if (tFar < t)
    {
        // 没进入体素网格 → 天空
        float sky = pow(rayDir.y * 0.5 + 0.5, 2.0);
        return float4(sky * float3(0.5, 0.7, 1.0), 1.0);
    }

    int3 cell = int3(floor(cam + rayDir * t));
    int3 step = int3(sign(rayDir));
    float3 tDelta = abs(invDir);
    float3 tNext = (float3(cell) + saturate(float3(step)) - cam) * invDir;
    
    // 记录进入体素的方向 → 用来算法线
    int3 hitNormalSign = int3(0, 0, 0);
    
    for (int i = 0; i < 128; i++)
    {
        if (t > tFar)
            break;
        if (any(cell < 0) || cell.x >= 32 || cell.y >= 32 || cell.z >= 48)
            break;
        
        float4 voxel = voxelGrid.Load(int4(cell, 0));
        
        if (voxel.w > 0.5)
        {
            // 命中！
            // 法线 = 进入体素的方向的反方向
            float3 normal = float3(hitNormalSign);

            // 自发光（任何模式都直接返回）
            if (voxel.w > 1.5)
                return float4(voxel.xyz, 1.0);

#if VIZ_MODE == 1
            // 只显示 albedo + 简单方向光（N·L），用来确认 voxel build + DDA 是对的
            float3 L = normalize(float3(0.5, 1.0, -0.3));
            float ndotl = saturate(dot(normal, L)) * 0.7 + 0.3; // ambient 0.3
            return float4(voxel.xyz * ndotl, 1.0);
#elif VIZ_MODE == 2
            // 法线方向可视化
            float3 nVis = normal * 0.5 + 0.5;
            return float4(nVis, 1.0);
#else
            // 完整 GI：cascade0 间接光
            // 注意：cell 是当前命中的固体体素。hitNormalSign 是 ray 步进的方向(step)。
            // 我们需要采样的是固体表面的那颗空体素！所以是 cell - step (即退回上一步的空气中)
            int3 probePos = cell + hitNormalSign; 
            float3 indirect = SampleIndirect(probePos, normal);
            float3 color = voxel.xyz * indirect * 2;
            color = color / (color + 1.0);
            return float4(pow(color, 1.0 / 2.2), 1.0);
#endif
        }
        
        // DDA 步进 + 记录从哪个方向进入
        hitNormalSign = int3(0, 0, 0);
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
    
    // 没命中 → 天空
    float sky = pow(rayDir.y * 0.5 + 0.5, 2.0);
    return float4(sky * float3(0.5, 0.7, 1.0), 1.0);
}