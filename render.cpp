#include "render.h"


#include <iostream>
#include <tchar.h>
#include "MathHelper.h"
#include <DirectXColors.h>

#include "Timer.h"
#include "DXShader.h"
#include "DXRootSignature.h"
#include "DXMaterial.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"




extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK GlobalWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    auto& camera = DXRender::GetInstance().GetMainCamera();
    if (!ImGui::GetCurrentContext() || !ImGui::GetIO().WantCaptureMouse)
    {
        camera.CameraWindowProc(hwnd, msg, wParam, lParam);
    }
    //camera.CameraWindowProc(hwnd, msg, wParam, lParam);
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

DXRender& DXRender::GetInstance()
{
    static DXRender instance;
    return instance;
}

void DXRender::Init(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Init Windows
    // 初始化窗口类和函数
    WNDCLASS wc = {};
    //wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MyWindowClass";
    wc.lpfnWndProc = GlobalWndProc;
    RegisterClass(&wc);

    // 创建窗口
    HWND hwnd = CreateWindow(
        L"MyWindowClass", L"My DX12 App",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600, nullptr, nullptr, hInstance, this);

    ShowWindow(hwnd, nCmdShow);

    InitDX(hwnd);

    // imgui - 在 InitDX 之后初始化
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1; // ImGui 只需要一个位置存字体贴图
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // 必须是 Shader 可见的
    Device::GetInstance().GetD3DDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&ImguiSrvHeap));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.Fonts->AddFontDefault();
    io.Fonts->Build();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);

    HRESULT hr = ImGui_ImplDX12_Init(Device::GetInstance().GetD3DDevice(), FrameBufferCount,
        DXGI_FORMAT_R8G8B8A8_UNORM, ImguiSrvHeap.Get(),
        ImguiSrvHeap->GetCPUDescriptorHandleForHeapStart(),
        ImguiSrvHeap->GetGPUDescriptorHandleForHeapStart());
    if (FAILED(hr)) {
        MessageBox(NULL, L"ImGui_ImplDX12_Init Failed!", L"Error", MB_OK);
        return;
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

    InitHandleSize();
    InitCommand();
    InitSwapChain(hWnd);
    InitRenderTargetHeapAndView();


    InitRootSignature();
    CompileShader();

    // 用Materila替代PSO
    // InitPSO();
    // shader应该也归属于material
    InitMaterial();

    CreateFence();
    ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));

    CreateConstantBufferView();

    // VIEWPORT and SCISSOR
    InitViewportAndScissor();
    InitDepthStencilBuffer();

    // Triangle.InitVertexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());
    // BoxShape.InitVertexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());

    for (int i = 0; i <= 5; ++i)
    {
        auto BoxPtr = new Box();
        BoxPtr->SetPosition(i * 3.0f - 7.f, 0.0f, 0.0f);
        BoxPtr->InitVertexBufferAndIndexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());
        BoxPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), SrvUavDescriptorSize, i);
        MeshList.push_back(BoxPtr);
    }

    for (int i = 0; i < 6; ++i)
    {
		auto SpherePtr = new Sphere();
		SpherePtr->SetPosition(i * 3.0f - 7.f, 3.0f, 0.0f);
		SpherePtr->InitVertexBufferAndIndexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());
		SpherePtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), SrvUavDescriptorSize, i + 6);
		MeshList.push_back(SpherePtr);
	}

	auto PanelPtr = new Plane(10,10,10,10);
	PanelPtr->SetPosition(0.0f, -2.0f, 0.0f);
	PanelPtr->InitVertexBufferAndIndexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());
	PanelPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), SrvUavDescriptorSize, 12);
	MeshList.push_back(PanelPtr);


    PtrMesh = new Box();
    PtrMesh->InitVertexBufferAndIndexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());

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
    RtvDescriptorSize = Device::GetInstance().GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    DsvDescriptorSize = Device::GetInstance().GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    SrvUavDescriptorSize = Device::GetInstance().GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DXRender::InitCommand()
{
    //创建命令队列
    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&CommandQueue)));
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator)));
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocator.Get(), nullptr, IID_PPV_ARGS(CommandList.GetAddressOf())));
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

    ThrowIfFailed(Device::GetInstance().GetDxgiFactory()->CreateSwapChainForHwnd(
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
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateDescriptorHeap(
        &RtvHeapDesc, IID_PPV_ARGS(&RtvHeap)
    ));
    // RTV Create 
    D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle = RtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (unsigned int i = 0; i < FrameBufferCount; i++)
    {
        ThrowIfFailed(SwapChain3->GetBuffer(i, IID_PPV_ARGS(&RenderTargets[i])));
        Device::GetInstance().GetD3DDevice()->CreateRenderTargetView(RenderTargets[i].Get(), nullptr, RtvHandle);
        RtvHandle.ptr += RtvDescriptorSize;
    }

}

void DXRender::CreateConstantBufferView()
{
    D3D12_DESCRIPTOR_HEAP_DESC ConstantBufferViewHeapDesc = {};
    ConstantBufferViewHeapDesc.NumDescriptors = 20;
    ConstantBufferViewHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ConstantBufferViewHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateDescriptorHeap(&ConstantBufferViewHeapDesc, IID_PPV_ARGS(&ConstantBufferViewHeap)));

    // 创建常量缓冲区 - 大小必须是256字节对齐 转移到 MeshBase
 //   const UINT constantBufferSize = (sizeof(ObjectConstants) + 255) & ~255;

    //CD3DX12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    //CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
 //   ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateCommittedResource(&HeapProps,D3D12_HEAP_FLAG_NONE,&BufferDesc,
 //       D3D12_RESOURCE_STATE_GENERIC_READ,
 //       nullptr,
 //       IID_PPV_ARGS(&ObjectConstantBuffer)));
 //   // 映射并初始化
 //   CD3DX12_RANGE readRange(0, 0);
 //   ThrowIfFailed(ObjectConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&ConstantBufferMappedData)));
 //   // ZeroMemory(ConstantBufferMappedData, constantBufferSize);
 //   // 创建CBV
 //   D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
 //   cbvDesc.BufferLocation = ObjectConstantBuffer->GetGPUVirtualAddress();
 //   cbvDesc.SizeInBytes = constantBufferSize; // 必须是256字节对齐
    //Device::GetInstance().GetD3DDevice()->CreateConstantBufferView(&cbvDesc, ConstantBufferViewHeap->GetCPUDescriptorHandleForHeapStart());

	// Light Constant Buffer
	const UINT LightConstantBufferSize = (sizeof(LightConstants) + 255) & ~255;

    // 创建上传堆的常量缓冲资源
    CD3DX12_HEAP_PROPERTIES LightHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(LightConstantBufferSize);
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateCommittedResource(&LightHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
		IID_PPV_ARGS(&LightConstantBuffer)));
    // 2) 映射得到 CPU 可写指针
    CD3DX12_RANGE ReadRange(0, 0);
    ThrowIfFailed(LightConstantBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&LightConstantBufferMappedData)));
	LightCbvCpuHandle = ConstantBufferViewHeap->GetCPUDescriptorHandleForHeapStart();
    LightCbvCpuHandle.ptr += /* LightCbvHeapIndex */ 19 * SrvUavDescriptorSize;
    LightCbvGpuHandle = ConstantBufferViewHeap->GetGPUDescriptorHandleForHeapStart();
    LightCbvGpuHandle.ptr += /* LightCbvHeapIndex */ 19 * SrvUavDescriptorSize;
    // 创建 CBV 
    D3D12_CONSTANT_BUFFER_VIEW_DESC LightCbvDesc = {};
    LightCbvDesc.BufferLocation = LightConstantBuffer->GetGPUVirtualAddress();
    LightCbvDesc.SizeInBytes = LightConstantBufferSize;
	Device::GetInstance().GetD3DDevice()->CreateConstantBufferView(&LightCbvDesc, LightCbvCpuHandle);


	// Material Constant Buffer
    const UINT MaterialConstantBufferSize = (sizeof(MaterialConstants) + 255) & ~255;
    CD3DX12_HEAP_PROPERTIES MatHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC MatBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(MaterialConstantBufferSize);
	ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateCommittedResource(&MatHeapProps, D3D12_HEAP_FLAG_NONE, &MatBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&MaterialConstantBuffer)));
    CD3DX12_RANGE MatReadRange(0, 0);
    ThrowIfFailed(MaterialConstantBuffer->Map(0, &MatReadRange, reinterpret_cast<void**>(&MaterialConstantBufferMappedData)));
	MaterialCbvCpuHandle = ConstantBufferViewHeap->GetCPUDescriptorHandleForHeapStart();
	MaterialCbvCpuHandle.ptr += MaterialCbvHeapIndex * SrvUavDescriptorSize;
	MaterialCbvGpuHandle = ConstantBufferViewHeap->GetGPUDescriptorHandleForHeapStart();
	MaterialCbvGpuHandle.ptr += MaterialCbvHeapIndex * SrvUavDescriptorSize;
	D3D12_CONSTANT_BUFFER_VIEW_DESC MaterialCbvDesc = {};
	MaterialCbvDesc.BufferLocation = MaterialConstantBuffer->GetGPUVirtualAddress();
	MaterialCbvDesc.SizeInBytes = MaterialConstantBufferSize;
    Device::GetInstance().GetD3DDevice()->CreateConstantBufferView(&MaterialCbvDesc, MaterialCbvCpuHandle);


}

void DXRender::InitRootSignature()
{
    // 跟参数
	DXRootSignature rootSigBuilder;
    // b0 constant
	rootSigBuilder.AddCBVDescriptorTable(0, D3D12_SHADER_VISIBILITY_ALL);
	// b1 light constant cbPerFrame
	rootSigBuilder.AddCBVDescriptorTable(1, D3D12_SHADER_VISIBILITY_ALL);
    // b2 material constant
    rootSigBuilder.AddCBVDescriptorTable(2, D3D12_SHADER_VISIBILITY_ALL);
    RootSignature = rootSigBuilder.Build(Device::GetInstance().GetD3DDevice());

}

void DXRender::CompileShader()
{

	DXShaderManager::GetInstance().CreateOrFindShader(L"TestVS", L"PBRShader.hlsl", "VSMain", "vs_5_0");
    DXShaderManager::GetInstance().CreateOrFindShader(L"TestPS", L"PBRShader.hlsl", "PSMain", "ps_5_0");

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
    //D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    //{
    //    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    //    { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    //};



    // PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc = {};
    PsoDesc.InputLayout = { StandardVertexInputLayout, _countof(StandardVertexInputLayout) };
    PsoDesc.pRootSignature = RootSignature.Get();
    //PsoDesc.VS = { reinterpret_cast<BYTE*>(VS->GetBufferPointer()), VS->GetBufferSize() };
    //PsoDesc.PS = { reinterpret_cast<BYTE*>(PS->GetBufferPointer()), PS->GetBufferSize() };

	auto TESTVS = DXShaderManager::GetInstance().GetShaderByName(L"TestVS")->GetBytecode();
	auto TESTPS = DXShaderManager::GetInstance().GetShaderByName(L"TestPS")->GetBytecode();

	PsoDesc.VS = { reinterpret_cast<BYTE*>(TESTVS->GetBufferPointer()), TESTVS->GetBufferSize()};
	PsoDesc.PS = { reinterpret_cast<BYTE*>(TESTPS->GetBufferPointer()), TESTPS->GetBufferSize() };

    PsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    PsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    //PsoDesc.DepthStencilState.DepthEnable = TRUE;
    PsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    PsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    PsoDesc.SampleMask = UINT_MAX;
    PsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    PsoDesc.NumRenderTargets = 1;
    PsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    PsoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&PipelineState)));

}

void DXRender::InitMaterial()
{
	// 2025.12.04 不再使用TestInputLayout，使用Common.h中的StandardVertexInputLayout 对应的Vertex结构体：StandardVertex
    // 输入布局描述
    //D3D12_INPUT_ELEMENT_DESC TestInputLayout[] =
    //{
    //    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    //    { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    //};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC TestPsoDesc = {};
    TestPsoDesc.InputLayout = { StandardVertexInputLayout, _countof(StandardVertexInputLayout) };
    TestPsoDesc.pRootSignature = RootSignature.Get();
    auto TESTVS = DXShaderManager::GetInstance().GetShaderByName(L"TestVS")->GetBytecode();
    auto TESTPS = DXShaderManager::GetInstance().GetShaderByName(L"TestPS")->GetBytecode();

    TestPsoDesc.VS = { reinterpret_cast<BYTE*>(TESTVS->GetBufferPointer()), TESTVS->GetBufferSize() };
    TestPsoDesc.PS = { reinterpret_cast<BYTE*>(TESTPS->GetBufferPointer()), TESTPS->GetBufferSize() };
    TestPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    TestPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	TestPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    TestPsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    TestPsoDesc.SampleMask = UINT_MAX;
    TestPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    TestPsoDesc.NumRenderTargets = 1;
    TestPsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    TestPsoDesc.SampleDesc.Count = 1;

	Material* TestMaterial = new Material("TestMaterial", RootSignature, TestPsoDesc);

}

void DXRender::CreateFence()
{
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));
}

void DXRender::Draw()
{
    ThrowIfFailed(CommandAllocator->Reset());
    //ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), PipelineState.Get()));
	Material& TestMaterial = MaterialManager::GetInstance().GetOrCreateMaterial("TestMaterial");

    ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), TestMaterial.GetPSO().Get()));


    CD3DX12_RESOURCE_BARRIER Barrier_P2RT = CD3DX12_RESOURCE_BARRIER::Transition(
        RenderTargets[CurrentFrameIdx].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    CommandList->ResourceBarrier(1, &Barrier_P2RT);

    D3D12_CPU_DESCRIPTOR_HANDLE CPU_RTV_Handle = RtvHeap->GetCPUDescriptorHandleForHeapStart();

    CPU_RTV_Handle.ptr += CurrentFrameIdx * RtvDescriptorSize;

	D3D12_CPU_DESCRIPTOR_HANDLE CPU_DSV_Handle = DsvHeap->GetCPUDescriptorHandleForHeapStart();

    // 添加清除渲染目标
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f }; // 深蓝色背景
    CommandList->ClearRenderTargetView(CPU_RTV_Handle, clearColor, 0, nullptr);
	CommandList->ClearDepthStencilView(CPU_DSV_Handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    CommandList->OMSetRenderTargets(1, &CPU_RTV_Handle, FALSE, &CPU_DSV_Handle);
    CommandList->RSSetViewports(1, &ScreenViewport);
    CommandList->RSSetScissorRects(1, &ScissorRect);

    CommandList->SetGraphicsRootSignature(RootSignature.Get());

    // 绑定描述符堆
    ID3D12DescriptorHeap* descriptorHeaps[] = { ConstantBufferViewHeap.Get() };
    CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    //Light Constants
	auto MainCameraPos = MainCamera.GetPosition();
    LightConstantInstance.CameraPosition = { MainCameraPos.x, MainCameraPos.y,MainCameraPos.z };

    // 将数据拷贝到 Map 好的内存中
    if (LightConstantBufferMappedData)
    {
        memcpy(LightConstantBufferMappedData, &LightConstantInstance, sizeof(LightConstants));
    }
    CommandList->SetGraphicsRootDescriptorTable(1, LightCbvGpuHandle);

	// Material Constants
    if (MaterialConstantBufferMappedData)
    {
        memcpy(MaterialConstantBufferMappedData, &MaterialConstantInstance, sizeof(MaterialConstants));
    }
    CommandList->SetGraphicsRootDescriptorTable(2, MaterialCbvGpuHandle);

    for (auto MeshElement : MeshList)
    {

        Timer::GetTimerInstance().StartNamedTimer("Draw");

        Timer::GetTimerInstance().StartNamedTimer("UpdateCB");

        {
            // 更新常量缓冲区
            auto MVPMatrix = MeshElement->CalMVPMatrix(MainCamera.CalViewProjMatrix());
            ObjectConstants objConstants;
            DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
            MeshElement->UpdateObjectConstantBuffer(objConstants);
        }
        Timer::GetTimerInstance().StopNamedTimer("UpdateCB");

        Timer::GetTimerInstance().StartNamedTimer("InitResource");
        // 绑定 CBV
        // CommandList->SetGraphicsRootDescriptorTable(0, ConstantBufferViewHeap->GetGPUDescriptorHandleForHeapStart());
        CommandList->SetGraphicsRootDescriptorTable(0, MeshElement->GetGbvCpuHandle());
        CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        auto VertexBufferView = MeshElement->GetVertexBufferView();
        CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
        auto IndexBufferView = MeshElement->GetIndexBufferView();
        CommandList->IASetIndexBuffer(&IndexBufferView);

        CommandList->DrawIndexedInstanced(MeshElement->GetIndexCount(), 1, 0, 0, 0);



        Timer::GetTimerInstance().StopNamedTimer("InitResource");
        Timer::GetTimerInstance().StartNamedTimer("ExecuteCommand");
        //ExecuteCommandAndWaitForComplete();
        Timer::GetTimerInstance().StopNamedTimer("ExecuteCommand");

    }

    // --- Start ImGui Frame ---
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // --- Build UI ---
    {
        ImGui::Begin("Debug Info");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
            1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        // You can add controls here later
        // ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f);

        // camera
		auto MainCameraPos = MainCamera.GetPosition();
        float MainCameraPosFloat3[3] = { MainCameraPos.x, MainCameraPos.y, MainCameraPos.z };
        if (ImGui::DragFloat3("Camera Position", MainCameraPosFloat3, 0.1f))
        {
			MainCamera.SetPosition(MainCameraPosFloat3[0], MainCameraPosFloat3[1], MainCameraPosFloat3[2]);
        }

		// light dir
		auto LightDir = LightConstantInstance.LightDirection;
		float LightDirFloat3[3] = { LightDir.x, LightDir.y, LightDir.z };
        if (ImGui::DragFloat3("Light Dir", LightDirFloat3, 0.02f))
        {
			LightConstantInstance.LightDirection = { LightDirFloat3[0], LightDirFloat3[1], LightDirFloat3[2] };
        }
		float Roughness = MaterialConstantInstance.Roughness;
        if (ImGui::DragFloat("Roughness", &Roughness, 0.02f, 0.05f, 1.0f))
        {
			MaterialConstantInstance.Roughness = Roughness;
        }

        ImGui::End();
    }
    ImGui::Render();
    
    // 切换到 ImGui 的 descriptor heap 并渲染
    ID3D12DescriptorHeap* imguiHeaps[] = { ImguiSrvHeap.Get()};
    CommandList->SetDescriptorHeaps(1, imguiHeaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), CommandList.Get());
    
    // --- 一帧结束 ---
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

    Timer::GetTimerInstance().StopNamedTimer("Draw");

}

DXRender::~DXRender()
{
    if (PtrMesh)
    {
        delete PtrMesh;
    }
}

DXRender::DXRender()
{
    MainCamera.Init((float)Width, (float)Height);
}

// DepthStencilBuffer
void DXRender::InitDepthStencilBuffer()
{
    D3D12_DESCRIPTOR_HEAP_DESC DsvHeapDesc = {};
    DsvHeapDesc.NumDescriptors = 1;
	DsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    DsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateDescriptorHeap(&DsvHeapDesc, IID_PPV_ARGS(&DsvHeap)));

    DXGI_FORMAT DepthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    D3D12_CLEAR_VALUE DepthOptimizedClearValue = {};
    DepthOptimizedClearValue.Format = DepthFormat;
    DepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    DepthOptimizedClearValue.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES DepthHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC DepthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DepthFormat,
        Width,
        Height,
        1, // 1 texture
        1  // 1 mipmap level
    );
    DepthResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateCommittedResource(
        &DepthHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &DepthResourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &DepthOptimizedClearValue,
        IID_PPV_ARGS(&DepthStencilBuffer)
    ));
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
    dsv.Format = DepthFormat;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle = DsvHeap->GetCPUDescriptorHandleForHeapStart();
    Device::GetInstance().GetD3DDevice()->CreateDepthStencilView(DepthStencilBuffer.Get(), &dsv, DsvHandle);
	DepthStencilBuffer->SetName(L"Depth Stencil Buffer");
}
// DepthStencilBuffer End
