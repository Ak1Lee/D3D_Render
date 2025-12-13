#include "Geometry.h"

DirectX::XMMATRIX MeshBase::GetWorldMatrix()
{
    // 缩放矩阵
    DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(Scale.x, Scale.y, Scale.z);

    // 旋转矩阵（欧拉角举例，注意顺序）
    DirectX::XMMATRIX rot = DirectX::XMMatrixRotationRollPitchYaw(Angle.x, Angle.y, Angle.z);

    // 平移矩阵
    DirectX::XMMATRIX trans = DirectX::XMMatrixTranslation(Pos.x, Pos.y, Pos.z);

    // 最终世界矩阵 = S * R * T
    auto WorldMatrix = scale * rot * trans;

    // 存到成员变量 World (XMFLOAT4X4)
    XMStoreFloat4x4(&World, WorldMatrix);

    return WorldMatrix; // 直接返回 XMMATRIX
}

DirectX::XMMATRIX MeshBase::CalMVPMatrix(DirectX::XMMATRIX ViewProj)
{
    return GetWorldMatrix() * ViewProj;
}

void MeshBase::InitObjectConstantBuffer(ID3D12Device* Device, ID3D12DescriptorHeap* GlobalConstantBufferViewHeap, UINT descriptorSize, UINT indexInHeap)
{
    const UINT constantBufferSize = (sizeof(ObjectConstants) + 255) & ~255;

    CD3DX12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);

    ThrowIfFailed(Device->CreateCommittedResource(
        &HeapProps,
        D3D12_HEAP_FLAG_NONE,
        &BufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&ObjectConstantBuffer)));

    // 映射并初始化
    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(ObjectConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&ConstantBufferMappedData)));


    // 计算这个 Mesh 在 Heap 中的 slot
    CbvCpuHandle = GlobalConstantBufferViewHeap->GetCPUDescriptorHandleForHeapStart();
    CbvCpuHandle.ptr += indexInHeap * descriptorSize;

    CbvGpuHandle = GlobalConstantBufferViewHeap->GetGPUDescriptorHandleForHeapStart();
    CbvGpuHandle.ptr += indexInHeap * descriptorSize;


    // 创建CBV
    D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc = {};
    CBVDesc.BufferLocation = ObjectConstantBuffer->GetGPUVirtualAddress();
    CBVDesc.SizeInBytes = constantBufferSize;
    Device->CreateConstantBufferView(&CBVDesc, CbvCpuHandle);

}

void MeshBase::UpdateObjectConstantBuffer(ObjectConstants& ObjConst)
{
    memcpy(ConstantBufferMappedData, &ObjConst, sizeof(ObjConst));
}

void MeshBase::CreateVertexAndIndexBufferHeap(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList)
{
    VertexCount = VertexList.size();
    // UINT VertexBufferSize = static_cast<UINT>(VertexList.size() * sizeof(SimpleVertex));
    UINT VertexBufferSize = static_cast<UINT>(VertexList.size() * sizeof(StandardVertex));
    IndexCount = IndiceList.size();
    UINT IndexBufferSize = static_cast<UINT>(IndiceList.size() * sizeof(std::uint16_t));

    /*************************************VBV********************************/
    // 创建CPU副本
    ThrowIfFailed(D3DCreateBlob(VertexBufferSize, &VertexBufferCPU));
    memcpy(VertexBufferCPU->GetBufferPointer(), VertexList.data(), VertexBufferSize);

    // 创建默认堆Default Heap
    CD3DX12_HEAP_PROPERTIES VertexDefaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC VertexDefaultHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(VertexBufferSize);;

    Device->CreateCommittedResource(
        &VertexDefaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &VertexDefaultHeapDesc,
        D3D12_RESOURCE_STATE_COMMON, // 初始状态：准备接收复制的数据
        nullptr,
        IID_PPV_ARGS(&VertexDefaultBufferGPU)
    );

    // 创建 Upload Heap 中的临时缓冲区
    CD3DX12_HEAP_PROPERTIES VertexUploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    Device->CreateCommittedResource(
        &VertexUploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &VertexDefaultHeapDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&VertexUploadBuffer)
    );

    // 顶点数据->upload
    UINT8* VertexDataBegin = nullptr;
    CD3DX12_RANGE readRange(0, 0);
    // 映射
    ThrowIfFailed(VertexUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&VertexDataBegin)));
    // memcopy
    memcpy(VertexDataBegin, VertexList.data(), VertexBufferSize);
    VertexUploadBuffer->Unmap(0, nullptr);

    //upload-> default
    // 添加转换到COPY_DEST状态的Barrier
    CD3DX12_RESOURCE_BARRIER PreCopyBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        VertexDefaultBufferGPU.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST
    );
    CommandList->ResourceBarrier(1, &PreCopyBarrier);
    CommandList->CopyBufferRegion(
        VertexDefaultBufferGPU.Get(),
        0,
        VertexUploadBuffer.Get(),
        0,
        VertexBufferSize
    );
    // DefaultBuffer 从 COPY_DEST 到 VERTEX_AND_CONSTANT_BUFFER
    CD3DX12_RESOURCE_BARRIER PostCopyBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        VertexDefaultBufferGPU.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    );
    CommandList->ResourceBarrier(1, &PostCopyBarrier);

    VertexBufferView.BufferLocation = VertexDefaultBufferGPU->GetGPUVirtualAddress();
    VertexBufferView.StrideInBytes = sizeof(StandardVertex);
    VertexBufferView.SizeInBytes = VertexBufferSize;

    /*************************************IBV********************************/
    // 创建CPU副本

    // 创建默认堆Default Heap
    CD3DX12_HEAP_PROPERTIES IndexDefaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    //desc-commitresource
    CD3DX12_RESOURCE_DESC IndexDefaultHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(IndexBufferSize);
    Device->CreateCommittedResource(
        &IndexDefaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &IndexDefaultHeapDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&IndexDefaultBufferGPU));


    // 创建 Upload Heap 中的临时缓冲区
    CD3DX12_RESOURCE_DESC IndexUploadHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(IndexBufferSize);
    CD3DX12_HEAP_PROPERTIES IndexUploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    //desc-commitresource
    Device->CreateCommittedResource(
        &IndexUploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &IndexUploadHeapDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&IndexUploadBuffer));


    // 顶点数据->upload
    UINT8* IndexDataBegin = nullptr;
    CD3DX12_RANGE IndexReadRange(0, 0);
    // Map
    // memcpy
    IndexUploadBuffer->Map(0, &IndexReadRange, reinterpret_cast<void**>(&IndexDataBegin));
    memcpy(IndexDataBegin, IndiceList.data(), IndexBufferSize);

    //upload-> default
    // PreCopyBarrier
    CD3DX12_RESOURCE_BARRIER PreCopyBarrier_2 = CD3DX12_RESOURCE_BARRIER::Transition(
        IndexDefaultBufferGPU.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST
    );
    CommandList->ResourceBarrier(1, &PreCopyBarrier_2);
    CommandList->CopyBufferRegion(
        IndexDefaultBufferGPU.Get(),
        0,
        IndexUploadBuffer.Get(),
        0,
        IndexBufferSize
    );
    IndexUploadBuffer->Unmap(0, nullptr);

    // PostCopyBarrier
    CD3DX12_RESOURCE_BARRIER PostCopyBarrier_2 = CD3DX12_RESOURCE_BARRIER::Transition(
        IndexDefaultBufferGPU.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_INDEX_BUFFER
    );
    CommandList->ResourceBarrier(1, &PostCopyBarrier_2);
    // 设置IBV
    IndexBufferView.BufferLocation = IndexDefaultBufferGPU->GetGPUVirtualAddress();
    IndexBufferView.SizeInBytes = IndexBufferSize;
    IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    IndexCount = IndiceList.size();
}


Box::Box(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) : MeshBase(Device, CommandList)
{
    InitVertexBufferAndIndexBuffer(Device, CommandList);
}

void Box::InitVertexBufferAndIndexBuffer(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList)
{

    auto cRed = DirectX::XMFLOAT4(DirectX::Colors::Red);
    auto cGreen = DirectX::XMFLOAT4(DirectX::Colors::Green);
    auto cBlue = DirectX::XMFLOAT4(DirectX::Colors::Blue);
    auto cYellow = DirectX::XMFLOAT4(DirectX::Colors::Yellow);
    auto cCyan = DirectX::XMFLOAT4(DirectX::Colors::Cyan);
    auto cMagenta = DirectX::XMFLOAT4(DirectX::Colors::Magenta);
    auto cWhite = DirectX::XMFLOAT4(DirectX::Colors::White);
    auto cBlack = DirectX::XMFLOAT4(DirectX::Colors::Black);

    VertexList.push_back({ DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), cWhite }); // 0
    VertexList.push_back({ DirectX::XMFLOAT3(-1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), cBlack }); // 1
    VertexList.push_back({ DirectX::XMFLOAT3(1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), cRed }); // 2
    VertexList.push_back({ DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), cGreen }); // 3

    // 2. Back Face (Z = +1) -> Normal (0, 0, 1)
    VertexList.push_back({ DirectX::XMFLOAT3(1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), cMagenta }); // 4
    VertexList.push_back({ DirectX::XMFLOAT3(1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), cCyan }); // 5
    VertexList.push_back({ DirectX::XMFLOAT3(-1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), cYellow }); // 6
    VertexList.push_back({ DirectX::XMFLOAT3(-1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), cBlue }); // 7

    // 3. Top Face (Y = +1) -> Normal (0, 1, 0)
    VertexList.push_back({ DirectX::XMFLOAT3(-1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), cBlack }); // 8
    VertexList.push_back({ DirectX::XMFLOAT3(-1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), cYellow }); // 9
    VertexList.push_back({ DirectX::XMFLOAT3(1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), cCyan }); // 10
    VertexList.push_back({ DirectX::XMFLOAT3(1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), cRed }); // 11

    // 4. Bottom Face (Y = -1) -> Normal (0, -1, 0)
    VertexList.push_back({ DirectX::XMFLOAT3(-1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), cBlue }); // 12
    VertexList.push_back({ DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), cWhite }); // 13
    VertexList.push_back({ DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), cGreen }); // 14
    VertexList.push_back({ DirectX::XMFLOAT3(1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(0.0f, -1.0f, 0.0f), cMagenta }); // 15

    // 5. Left Face (X = -1) -> Normal (-1, 0, 0)
    VertexList.push_back({ DirectX::XMFLOAT3(-1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), cBlue }); // 16
    VertexList.push_back({ DirectX::XMFLOAT3(-1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), cYellow }); // 17
    VertexList.push_back({ DirectX::XMFLOAT3(-1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), cBlack }); // 18
    VertexList.push_back({ DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(-1.0f, 0.0f, 0.0f), cWhite }); // 19

    // 6. Right Face (X = +1) -> Normal (1, 0, 0)
    VertexList.push_back({ DirectX::XMFLOAT3(1.0f, -1.0f, -1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), cGreen }); // 20
    VertexList.push_back({ DirectX::XMFLOAT3(1.0f,  1.0f, -1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), cRed }); // 21
    VertexList.push_back({ DirectX::XMFLOAT3(1.0f,  1.0f,  1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), cCyan }); // 22
    VertexList.push_back({ DirectX::XMFLOAT3(1.0f, -1.0f,  1.0f), DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), cMagenta }); // 23

    for (int i = 0; i < 6; ++i)
    {
        // 每个面的起始顶点索引 (0, 4, 8, 12, 16, 20)
        std::uint16_t baseIndex = i * 4;

        // Triangle 1
        IndiceList.push_back(baseIndex + 0);
        IndiceList.push_back(baseIndex + 1);
        IndiceList.push_back(baseIndex + 2);

        // Triangle 2
        IndiceList.push_back(baseIndex + 0);
        IndiceList.push_back(baseIndex + 2);
        IndiceList.push_back(baseIndex + 3);
    }

    //VertexList.clear();
    //{
    //    VertexList.push_back({ DirectX::XMFLOAT3(-1.0, -1.0f, -1.0),DirectX::XMFLOAT4(DirectX::Colors::White) });
    //    VertexList.push_back({ DirectX::XMFLOAT3(-1.0, +1.0f, -1.0),DirectX::XMFLOAT4(DirectX::Colors::Black) });
    //    VertexList.push_back({ DirectX::XMFLOAT3(+1.0, +1.0f, -1.0),DirectX::XMFLOAT4(DirectX::Colors::Red) });
    //    VertexList.push_back({ DirectX::XMFLOAT3(+1.0, -1.0f, -1.0),DirectX::XMFLOAT4(DirectX::Colors::Green) });
    //    VertexList.push_back({ DirectX::XMFLOAT3(-1.0, -1.0f, +1.0),DirectX::XMFLOAT4(DirectX::Colors::Blue) });
    //    VertexList.push_back({ DirectX::XMFLOAT3(-1.0, +1.0f, +1.0),DirectX::XMFLOAT4(DirectX::Colors::Yellow) });
    //    VertexList.push_back({ DirectX::XMFLOAT3(+1.0, +1.0f, +1.0),DirectX::XMFLOAT4(DirectX::Colors::Cyan) });
    //    VertexList.push_back({ DirectX::XMFLOAT3(+1.0, -1.0f, +1.0),DirectX::XMFLOAT4(DirectX::Colors::Magenta) });
    //}


    //IndiceList =
    //{
    //    // front face
    //    0, 1, 2,
    //    0, 2, 3,

    //    // back face
    //    4, 6, 5,
    //    4, 7, 6,

    //    // left face
    //    4, 5, 1,
    //    4, 1, 0,

    //    // right face
    //    3, 2, 6,
    //    3, 6, 7,

    //    // top face
    //    1, 5, 6,
    //    1, 6, 2,

    //    // bottom face
    //    4, 0, 3,
    //    4, 3, 7
    //};

    CreateVertexAndIndexBufferHeap(Device, CommandList);

    VertexDefaultBufferGPU->SetName(L"BoxMesh Default Vertex Buffer");
    VertexUploadBuffer->SetName(L"BoxMesh Upload Vertex Buffer");
    IndexDefaultBufferGPU->SetName(L"BoxMesh Default Index Buffer");
    IndexUploadBuffer->SetName(L"BoxMesh Upload Index Buffer");

}

void Sphere::InitVertexBufferAndIndexBuffer(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList)
{
	VertexList.clear();
	IndiceList.clear();

    // 顶点
	float PhiStep = DirectX::XM_PI / Stacks;
	float ThetaStep = 2.0f * DirectX::XM_PI / Slices;

    for(auto i = 0; i <= Stacks; ++i)
    {
        float Phi = i * PhiStep;
        for(auto j = 0; j <= Slices; ++j)
        {
			float  Theta = j * ThetaStep;

			float x = Radius * sinf(Phi) * cosf(Theta);
			float y = Radius * cosf(Phi);
			float z = Radius * sinf(Phi) * sinf(Theta);
            DirectX::XMFLOAT3 Pos(x, y, z);

			DirectX::XMVECTOR N = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&Pos));
            DirectX::XMFLOAT3 Normal;
			DirectX::XMStoreFloat3(&Normal, N);

            DirectX::XMFLOAT4 Color = {
                Normal.x * 0.5f + 0.5f,
                Normal.y * 0.5f + 0.5f,
                Normal.z * 0.5f + 0.5f,
                1.0f
            };

            VertexList.push_back({ Pos, Normal, Color });

        }

	}

    // 索引数据
    for(auto i = 0; i < Stacks; ++i)
    {
        for(auto j = 0; j < Slices; ++j)
        {
            UINT stride = Slices + 1;

            UINT topLeft = i * stride + j;
            UINT topRight = i * stride + (j + 1);
            UINT bottomLeft = (i + 1) * stride + j;
            UINT bottomRight = (i + 1) * stride + (j + 1);

            // 两个三角形组成一个Quad
            // 顺序：(TopLeft, BottomLeft, TopRight)
            IndiceList.push_back(topLeft);
            IndiceList.push_back(bottomLeft);
            IndiceList.push_back(topRight);

            // 顺序：(TopRight, BottomLeft, BottomRight)
            IndiceList.push_back(topRight);
            IndiceList.push_back(bottomLeft);
            IndiceList.push_back(bottomRight);
        }
	}
    CreateVertexAndIndexBufferHeap(Device, CommandList);


}

void Plane::InitVertexBufferAndIndexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    VertexList.clear();
    IndiceList.clear();

	float HalfWidth = 0.5f * Width;
	float HalfDepth = 0.5f * Depth;

	float dx = Width / static_cast<float>(M);
	float dz = Depth / static_cast<float>(N);

    for (UINT i = 0; i < M + 1; ++i)
    {
        // Z坐标：从 +Depth/2 递减到 -Depth/2 (或者反过来，取决于你的坐标系习惯)
        // 这里采用 Z 从正到负，对应纹理坐标 V 从 0 到 1
        float z = HalfDepth - i * dz;

        for (UINT j = 0; j < N + 1; ++j)
        {
            // X坐标：从 -Width/2 递增到 +Width/2
            float x = -HalfWidth + j * dx;

            // Y坐标：平面躺在地上，所以 Y = 0
            DirectX::XMFLOAT3 pos(x, 0.0f, z);

            // --- 颜色生成 (棋盘格效果) ---
            // 根据 i 和 j 的奇偶性决定颜色，这样即使没有光照也能看清网格
            DirectX::XMFLOAT4 color;
            if ((i + j) % 2 == 0)
            {
                // 白色格
                color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            }
            else
            {
                // 灰色格 (区别于黑色背景)
                color = DirectX::XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
            }

            VertexList.push_back({ pos, color });
        }
    }

    // 2. 生成索引
    // 遍历每一个格子 (Quad)，每个格子切成两个三角形
    for (UINT i = 0; i < M; ++i)
    {
        for (UINT j = 0; j < N; ++j)
        {
            // 计算当前格子四个顶点的索引
            // 每一行有 (N+1) 个顶点
            UINT rowStride = N + 1;

            UINT topLeft = i * rowStride + j;
            UINT topRight = i * rowStride + (j + 1);
            UINT bottomLeft = (i + 1) * rowStride + j;
            UINT bottomRight = (i + 1) * rowStride + (j + 1);

            // 三角形 1: TopLeft -> TopRight -> BottomLeft
            IndiceList.push_back(topLeft);
            IndiceList.push_back(topRight);
            IndiceList.push_back(bottomLeft);

            // 三角形 2: BottomLeft -> TopRight -> BottomRight
            IndiceList.push_back(bottomLeft);
            IndiceList.push_back(topRight);
            IndiceList.push_back(bottomRight);
        }
    }

    // 3. 创建 GPU 资源 (调用基类方法)
    CreateVertexAndIndexBufferHeap(device, cmdList);

    // 4. 设置调试名
    if (VertexDefaultBufferGPU) VertexDefaultBufferGPU->SetName(L"Plane Vertex Buffer");
    if (IndexDefaultBufferGPU) IndexDefaultBufferGPU->SetName(L"Plane Index Buffer");
}
