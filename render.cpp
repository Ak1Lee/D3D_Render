#include "render.h"


#include <iostream>
#include <tchar.h>
#include "MathHelper.h"
#include <DirectXColors.h>



void Device::Init()
{
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&DxgiFactory)));

    // 创建device
    HRESULT HardwareCreateResult = D3D12CreateDevice(
        nullptr,             // 默认适配器
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&D3DDevice)
    );

    if (HardwareCreateResult == E_NOINTERFACE)
    {
        std::cout << "Direct3D 12 is not supported on this device" << std::endl;

        std::cout << " Create Software Simulation" << std::endl;
        Microsoft::WRL::ComPtr<IDXGIAdapter> WARPAdapter;
        ThrowIfFailed(DxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&WARPAdapter)));
    }
    else
    {
        ThrowIfFailed(HardwareCreateResult);
        std::cout << " D3D12 Create Device Success!" << std::endl;
    }
}

void DXRender::InitDX(HWND hWnd)
{

#if defined(DEBUG) || defined(_DEBUG) || 1
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif


	MainCamera.Init((float)Width, (float)Height);

	DxDevice.Init();
    InitHandleSize();
    InitCommand();
    InitSwapChain(hWnd);
    InitRenderTargetHeapAndView();


    InitRootSignature();
    CompileShader();
    InitPSO();
    CreateFence();
    ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));

	CreateConstantBufferView();

    // VIEWPORT and SCISSOR
    InitViewportAndScissor();

    Triangle.InitVertexBuffer(DxDevice.GetD3DDevice(), CommandList.Get());
	Box.InitVertexBuffer(DxDevice.GetD3DDevice(), CommandList.Get());

    // 执行初始化命令
    ExecuteCommandAndWaitForComplete();
}

void DXRender::ExecuteCommandAndWaitForComplete()
{
    ThrowIfFailed(CommandList->Close());
    ID3D12CommandList* cmdLists[] = { CommandList.Get() };
    CommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // 等待GPU完成初始化
    const UINT64 initFenceValue = ++FenceValue;
    ThrowIfFailed(CommandQueue->Signal(Fence.Get(), initFenceValue));

    if (Fence->GetCompletedValue() < initFenceValue)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
        ThrowIfFailed(Fence->SetEventOnCompletion(initFenceValue, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void DXRender::InitViewportAndScissor()
{
    ScreenViewport.TopLeftX = 0;
    ScreenViewport.TopLeftY = 0;
    ScreenViewport.Width = static_cast<float>(Width);
    ScreenViewport.Height = static_cast<float>(Height);
    ScreenViewport.MinDepth = 0.0f;
    ScreenViewport.MaxDepth = 1.0f;
    ScissorRect.left = 0;
    ScissorRect.top = 0;
    ScissorRect.right = Width;
    ScissorRect.bottom = Height;
}

void DXRender::InitHandleSize()
{
    RtvDescriptorSize = DxDevice.GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    DsvDescriptorSize = DxDevice.GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	SrvUavDescriptorSize = DxDevice.GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DXRender::InitCommand()
{
    //创建命令队列
    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(DxDevice.GetD3DDevice()->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&CommandQueue)));
    ThrowIfFailed(DxDevice.GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator)));
    ThrowIfFailed(DxDevice.GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocator.Get(), nullptr, IID_PPV_ARGS(CommandList.GetAddressOf())));
	CommandList->Close();
}

void DXRender::InitSwapChain(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
    SwapChainDesc.BufferCount = FrameBufferCount;
    SwapChainDesc.Width = Width;
    SwapChainDesc.Height = Height;
    SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    SwapChainDesc.SampleDesc.Count = 1;
   
    ThrowIfFailed(DxDevice.GetDxgiFactory()->CreateSwapChainForHwnd(
        CommandQueue.Get(),
        hWnd,
        &SwapChainDesc,
        nullptr,
        nullptr,
        &SwapChain1
    ));
    ThrowIfFailed(SwapChain1.As(&SwapChain3));
	CurrentFrameIdx = SwapChain3->GetCurrentBackBufferIndex();
}

void DXRender::InitRenderTargetHeapAndView()
{
    // RenderTargetView Heap 和 RenderTargetView 描述符大小
    D3D12_DESCRIPTOR_HEAP_DESC RtvHeapDesc = {};
    RtvHeapDesc.NumDescriptors = FrameBufferCount;
    RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(DxDevice.GetD3DDevice()->CreateDescriptorHeap(
        &RtvHeapDesc, IID_PPV_ARGS(&RtvHeap)
    ));
    // RTV Create 
    D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle = RtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (unsigned int i = 0; i < FrameBufferCount; i++)
    {
        ThrowIfFailed(SwapChain3->GetBuffer(i, IID_PPV_ARGS(&RenderTargets[i])));
        DxDevice.GetD3DDevice()->CreateRenderTargetView(RenderTargets[i].Get(), nullptr, RtvHandle);
        RtvHandle.ptr += RtvDescriptorSize;
    }

}

void DXRender::CreateConstantBufferView()
{
	D3D12_DESCRIPTOR_HEAP_DESC ConstantBufferViewHeapDesc = {};
	ConstantBufferViewHeapDesc.NumDescriptors = 1;
	ConstantBufferViewHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ConstantBufferViewHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(DxDevice.GetD3DDevice()->CreateDescriptorHeap(&ConstantBufferViewHeapDesc, IID_PPV_ARGS(&ConstantBufferViewHeap)));

    // 创建常量缓冲区 - 大小必须是256字节对齐
    const UINT constantBufferSize = (sizeof(ObjectConstants) + 255) & ~255;

	CD3DX12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
    ThrowIfFailed(DxDevice.GetD3DDevice()->CreateCommittedResource(
        &HeapProps,
        D3D12_HEAP_FLAG_NONE,
        &BufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&ObjectConstantBuffer)));
    // 映射并初始化
    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(ObjectConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&ConstantBufferMappedData)));
    // ZeroMemory(ConstantBufferMappedData, constantBufferSize);
    // 创建CBV
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = ObjectConstantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = constantBufferSize; // 必须是256字节对齐
	DxDevice.GetD3DDevice()->CreateConstantBufferView(&cbvDesc, ConstantBufferViewHeap->GetCPUDescriptorHandleForHeapStart());

}

void DXRender::InitRootSignature()
{
    // 跟参数
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	
    CD3DX12_DESCRIPTOR_RANGE ConstantBufferViewTable;
	ConstantBufferViewTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	slotRootParameter[0].InitAsDescriptorTable(1, &ConstantBufferViewTable, D3D12_SHADER_VISIBILITY_ALL);


    CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc;
    RootSignatureDesc.Init(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ThrowIfFailed(D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, nullptr));
    ThrowIfFailed(DxDevice.GetD3DDevice()->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), IID_PPV_ARGS(&RootSignature)));
}

void DXRender::CompileShader()
{
#if defined(_DEBUG)
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif
    TCHAR shaderPath[] = _T("Shaders.hlsl");
    ThrowIfFailed(D3DCompileFromFile(shaderPath, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &VS, nullptr));
    ThrowIfFailed(D3DCompileFromFile(shaderPath, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &PS, nullptr));


}

void DXRender::InitInputLayout()
{
    // 输入布局描述
    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void DXRender::InitPSO()
{
    // 输入布局描述
    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc = {};
    PsoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    PsoDesc.pRootSignature = RootSignature.Get();
    PsoDesc.VS = { reinterpret_cast<BYTE*>(VS->GetBufferPointer()), VS->GetBufferSize() };
    PsoDesc.PS = { reinterpret_cast<BYTE*>(PS->GetBufferPointer()), PS->GetBufferSize() };
    PsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    PsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    PsoDesc.DepthStencilState.DepthEnable = FALSE;
    PsoDesc.DepthStencilState.StencilEnable = FALSE;
    PsoDesc.SampleMask = UINT_MAX;
    PsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PsoDesc.NumRenderTargets = 1;
    PsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    PsoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(DxDevice.GetD3DDevice()->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&PipelineState)));

}

void DXRender::CreateFence()
{
    ThrowIfFailed(DxDevice.GetD3DDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));
}



void RenderableItem::InitVertexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* CommandList)
{
    VertexList.clear();

    VertexList.push_back({ {0.0f, 0.25f, 0.0f }, {1.0f, 0.0f, 0.0f, 1.0f} });
    VertexList.push_back({ { 0.25f, -0.25f, 0.0f }, {0.0f, 1.0f, 0.0f, 1.0f} });
    VertexList.push_back({ { -0.25f, -0.25f, 0.0f }, {0.0f, 0.0f, 1.0f, 1.0f} });

    VertexCount = VertexList.size();

    VertexBufferSize = static_cast<UINT>(VertexList.size() * sizeof(Vertex));

    // 创建默认堆Default Heap
    CD3DX12_HEAP_PROPERTIES DefaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC DefaultBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(VertexBufferSize);
    
    ThrowIfFailed(device->CreateCommittedResource(
        &DefaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &DefaultBufferDesc,
        D3D12_RESOURCE_STATE_COMMON, // 初始状态：准备接收复制的数据
        nullptr,
        IID_PPV_ARGS(&VertexDefaultBuffer)
    ));

    // 创建 Upload Heap 中的临时缓冲区
    CD3DX12_HEAP_PROPERTIES UploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(
        &UploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &DefaultBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&VertexUploadBuffer)
    ));

    // 顶点数据->upload
    UINT8* vertexDataBegin = nullptr;
    CD3DX12_RANGE readRange(0, 0);  // 我们不会从这个资源读取，所以范围为空

    ThrowIfFailed(VertexUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin)));
    memcpy(vertexDataBegin, VertexList.data(), VertexBufferSize);
    VertexUploadBuffer->Unmap(0, nullptr);

    // 添加转换到COPY_DEST状态的Barrier
    CD3DX12_RESOURCE_BARRIER PreCopyBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        VertexDefaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST
    );
    CommandList->ResourceBarrier(1, &PreCopyBarrier);

    // UpLoad -> Default
    CommandList->CopyBufferRegion(
        VertexDefaultBuffer.Get(),
        0,
        VertexUploadBuffer.Get(),
        0,
        VertexBufferSize
    );

    // DefaultBuffer 从 COPY_DEST 到 VERTEX_AND_CONSTANT_BUFFER
    CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        VertexDefaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    );

    CommandList->ResourceBarrier(1, &Barrier);


    // 设置vbv
    VertexBufferView.BufferLocation = VertexDefaultBuffer->GetGPUVirtualAddress();
    VertexBufferView.StrideInBytes = sizeof(Vertex);
    VertexBufferView.SizeInBytes = VertexBufferSize;

    VertexDefaultBuffer->SetName(L"RenderableItem Default Vertex Buffer");
    VertexUploadBuffer->SetName(L"RenderableItem Upload Vertex Buffer");


}


void BoxMesh::InitVertexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* CommandList)
{
    VertexList.clear();
    {
        VertexList.push_back({ DirectX::XMFLOAT3(-1.0, -1.0f, -1.0),DirectX::XMFLOAT4(DirectX::Colors::White) });
        VertexList.push_back({ DirectX::XMFLOAT3(-1.0, +1.0f, -1.0),DirectX::XMFLOAT4(DirectX::Colors::Black) });
        VertexList.push_back({ DirectX::XMFLOAT3(+1.0, +1.0f, -1.0),DirectX::XMFLOAT4(DirectX::Colors::Red) });
        VertexList.push_back({ DirectX::XMFLOAT3(+1.0, -1.0f, -1.0),DirectX::XMFLOAT4(DirectX::Colors::Green) });
        VertexList.push_back({ DirectX::XMFLOAT3(-1.0, -1.0f, +1.0),DirectX::XMFLOAT4(DirectX::Colors::Blue) });
        VertexList.push_back({ DirectX::XMFLOAT3(-1.0, +1.0f, +1.0),DirectX::XMFLOAT4(DirectX::Colors::Yellow) });
        VertexList.push_back({ DirectX::XMFLOAT3(+1.0, +1.0f, +1.0),DirectX::XMFLOAT4(DirectX::Colors::Cyan) });
        VertexList.push_back({ DirectX::XMFLOAT3(+1.0, -1.0f, +1.0),DirectX::XMFLOAT4(DirectX::Colors::Magenta) });
    }


    std::vector<std::uint16_t> IndiceList =
    {
        // front face
        0, 1, 2,
        0, 2, 3,

        // back face
        4, 6, 5,
        4, 7, 6,

        // left face
        4, 5, 1,
        4, 1, 0,

        // right face
        3, 2, 6,
        3, 6, 7,

        // top face
        1, 5, 6,
        1, 6, 2,

        // bottom face
        4, 0, 3,
        4, 3, 7
    };

	VertexCount = VertexList.size();
	UINT VertexBufferSize = static_cast<UINT>(VertexList.size() * sizeof(Vertex));
	IndexCount = IndiceList.size();
	UINT IndexBufferSize = static_cast<UINT>(IndiceList.size() * sizeof(std::uint16_t));

    /*************************************VBV********************************/
    // 创建CPU副本
	ThrowIfFailed(D3DCreateBlob(VertexBufferSize, &VertexBufferCPU));
	memcpy(VertexBufferCPU->GetBufferPointer(), VertexList.data(), VertexBufferSize);

    // 创建默认堆Default Heap
	CD3DX12_HEAP_PROPERTIES VertexDefaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC VertexDefaultHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(VertexBufferSize);;

    device->CreateCommittedResource(
        &VertexDefaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &VertexDefaultHeapDesc,
        D3D12_RESOURCE_STATE_COMMON, // 初始状态：准备接收复制的数据
        nullptr,
        IID_PPV_ARGS(&VertexDefaultBuffer)
	);

	// 创建 Upload Heap 中的临时缓冲区
	CD3DX12_HEAP_PROPERTIES VertexUploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    device->CreateCommittedResource(
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
        VertexDefaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST
    );
    CommandList->ResourceBarrier(1, &PreCopyBarrier);
    CommandList->CopyBufferRegion(
        VertexDefaultBuffer.Get(),
        0,
        VertexUploadBuffer.Get(),
        0,
        VertexBufferSize
    );
	// DefaultBuffer 从 COPY_DEST 到 VERTEX_AND_CONSTANT_BUFFER
    CD3DX12_RESOURCE_BARRIER PostCopyBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
        VertexDefaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    );
	CommandList->ResourceBarrier(1, &PostCopyBarrier);

	VertexBufferView.BufferLocation = VertexDefaultBuffer->GetGPUVirtualAddress();
	VertexBufferView.StrideInBytes = sizeof(Vertex);
	VertexBufferView.SizeInBytes = VertexBufferSize;

    /*************************************IBV********************************/
    // 创建CPU副本

	// 创建默认堆Default Heap
    CD3DX12_HEAP_PROPERTIES IndexDefaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	//desc-commitresource
	CD3DX12_RESOURCE_DESC IndexDefaultHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(IndexBufferSize);
    device->CreateCommittedResource(
        &IndexDefaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &IndexDefaultHeapDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&IndexDefaultBuffer));


    // 创建 Upload Heap 中的临时缓冲区
	CD3DX12_RESOURCE_DESC IndexUploadHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(IndexBufferSize);
	CD3DX12_HEAP_PROPERTIES IndexUploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    //desc-commitresource
    device->CreateCommittedResource(
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
        IndexDefaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST
	);
    CommandList->ResourceBarrier(1, &PreCopyBarrier_2);
    CommandList->CopyBufferRegion(
        IndexDefaultBuffer.Get(),
        0,
        IndexUploadBuffer.Get(),
        0,
        IndexBufferSize
    );
    IndexUploadBuffer->Unmap(0, nullptr);





    // PostCopyBarrier
    CD3DX12_RESOURCE_BARRIER PostCopyBarrier_2 = CD3DX12_RESOURCE_BARRIER::Transition(
        IndexDefaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_INDEX_BUFFER
    );
    CommandList->ResourceBarrier(1, &PostCopyBarrier_2);
    // 设置IBV
    IndexBufferView.BufferLocation = IndexDefaultBuffer->GetGPUVirtualAddress();
	IndexBufferView.SizeInBytes = IndexBufferSize;
    IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    VertexDefaultBuffer->SetName(L"BoxMesh Default Vertex Buffer");
    VertexUploadBuffer->SetName(L"BoxMesh Upload Vertex Buffer");
    IndexDefaultBuffer->SetName(L"BoxMesh Default Index Buffer");
    IndexUploadBuffer->SetName(L"BoxMesh Upload Index Buffer");
    IndexCount = IndiceList.size();

}

DirectX::XMMATRIX BoxMesh::GetWorldMatrix()
{
    DirectX::XMMATRIX TempWorld = DirectX::XMLoadFloat4x4(&World);

    return TempWorld;
}

DirectX::XMMATRIX BoxMesh::CalMVPMatrix(DirectX::XMMATRIX ViewProj)
{
    return GetWorldMatrix() * ViewProj;
}

void DXRender::Draw()
{
    ThrowIfFailed(CommandAllocator->Reset());
    ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), PipelineState.Get()));

    {
        // 更新常量缓冲区
		auto MVPMatrix = Box.CalMVPMatrix(MainCamera.CalViewProjMatrix());
        ObjectConstants objConstants;
		DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
        //objConstants.WorldViewProj = MathHelper::Identity4x4();
		memcpy(ConstantBufferMappedData, &objConstants, sizeof(objConstants));

    }
    // 绑定描述符堆
    ID3D12DescriptorHeap* descriptorHeaps[] = { ConstantBufferViewHeap.Get() };
    CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);


    CommandList->RSSetViewports(1, &ScreenViewport);
    CommandList->RSSetScissorRects(1, &ScissorRect);

    CD3DX12_RESOURCE_BARRIER Barrier_P2RT = CD3DX12_RESOURCE_BARRIER::Transition(
        RenderTargets[CurrentFrameIdx].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    CommandList->ResourceBarrier(1, &Barrier_P2RT);

    D3D12_CPU_DESCRIPTOR_HANDLE CPU_RTV_Handle = RtvHeap->GetCPUDescriptorHandleForHeapStart();

    CPU_RTV_Handle.ptr += CurrentFrameIdx * RtvDescriptorSize;

    // 添加清除渲染目标
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f }; // 深蓝色背景
    CommandList->ClearRenderTargetView(CPU_RTV_Handle, clearColor, 0, nullptr);
    CommandList->OMSetRenderTargets(1, &CPU_RTV_Handle, FALSE, nullptr);
    CommandList->SetGraphicsRootSignature(RootSignature.Get());

    CommandList->SetGraphicsRootDescriptorTable(0, ConstantBufferViewHeap->GetGPUDescriptorHandleForHeapStart());

    CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //画三角形
    //auto VertexBufferView = Triangle.GetVertexBufferView();
    //CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
    //CommandList->DrawInstanced(Triangle.GetVertexCount(), 1, 0, 0);

    // 画box
	auto VertexBufferView = Box.GetVertexBufferView();
	CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
	auto IndexBufferView = Box.GetIndexBufferView();
	CommandList->IASetIndexBuffer(&IndexBufferView);
	CommandList->DrawIndexedInstanced(Box.GetIndexCount(), 1, 0, 0, 0);

    // 转换当前这个rt的状态到present
    CD3DX12_RESOURCE_BARRIER Barrier_RT2P = CD3DX12_RESOURCE_BARRIER::Transition(
        RenderTargets[CurrentFrameIdx].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    CommandList->ResourceBarrier(1, &Barrier_RT2P);

    ExecuteCommandAndWaitForComplete();

    ThrowIfFailed(SwapChain3->Present(1, 0));
    CurrentFrameIdx = SwapChain3->GetCurrentBackBufferIndex();
}