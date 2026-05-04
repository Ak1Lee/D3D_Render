RWTexture3D<float4> voxelGrid : register(u0);

[numthreads(8, 8, 8)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    float3 p = float3(id);
    float4 info = float4(0, 0, 0, 0); // 空气
    
    // 地板 (白)
    if (p.y < 1.0)
        info = float4(0.9, 0.9, 0.9, 1.0);
    
    // 天花板 (白)
    if (p.y > 30.0)
        info = float4(0.9, 0.9, 0.9, 1.0);
    
    // 后墙 (白)
    if (p.z > 28.0)
        info = float4(0.9, 0.9, 0.9, 1.0);
    
    // 左墙 (红)
    if (p.x < 1.0)
        info = float4(0.9, 0.2, 0.2, 1.0);
    
    // 右墙 (绿)
    if (p.x > 30.0)
        info = float4(0.2, 0.9, 0.2, 1.0);
    
    // 天花板灯 (自发光)
    if (p.y > 29.0 && p.x > 12.0 && p.x < 20.0 
                    && p.z > 12.0 && p.z < 20.0)
        info = float4(5.0, 5.8, 5.5, 2.0); // w=2 自发光
    
    // 小方块 (白)
    if (p.x > 5.0 && p.x < 12.0 
     && p.y > 0.0 && p.y < 10.0 
     && p.z > 8.0 && p.z < 15.0)
        info = float4(0.9, 0.9, 0.9, 1.0);
    
    // 高方块 (白)
    if (p.x > 18.0 && p.x < 25.0 
     && p.y > 0.0  && p.y < 20.0 
     && p.z > 16.0 && p.z < 23.0)
        info = float4(0.9, 0.9, 0.9, 1.0);
    
    voxelGrid[id] = info;
}