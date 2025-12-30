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

//tex load
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"




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

    // shadow
	InitShadowMap();
    InitShadowPSO();
	InitShadowMaskTexture();
    InitShadowMaskPSO();
    InitZPrepassPSO();
	InitPasses();
	Texture HDRTex("puresky");
	HDRTex.LoadHDRFromFile(CommandList.Get(), ".\\resources\\puresky_2k.hdr");

    

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



    CreateFence();
    ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));

    CreateConstantBufferView();

    // 用Materila替代PSO
    // InitPSO();
    // shader应该也归属于material
    InitMaterial();

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
		DescriptorAllocation AllocInfo = AllocateDescriptorHandle(SrvUavDescriptorSize);
        // BoxPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), SrvUavDescriptorSize, i);
        BoxPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), AllocInfo);
		BoxPtr->SetMaterialByName("Mat_Red");
        MeshList.push_back(BoxPtr);
    }

    for (int i = 0; i < 6; ++i)
    {
		auto SpherePtr = new Sphere();
		SpherePtr->SetPosition(i * 3.0f - 7.f, 3.0f, 0.0f);
		SpherePtr->InitVertexBufferAndIndexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());
		//SpherePtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), SrvUavDescriptorSize, i + 6);
        DescriptorAllocation AllocInfo = AllocateDescriptorHandle(SrvUavDescriptorSize);
        SpherePtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), AllocInfo);
		SpherePtr->SetMaterialByName("Mat_Gold");
		MeshList.push_back(SpherePtr);
	}

	//auto PanelPtr = new Plane(10,10,10,10);
	//PanelPtr->SetPosition(0.0f, -2.0f, 0.0f);
	//PanelPtr->InitVertexBufferAndIndexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());
 //   DescriptorAllocation AllocInfo = AllocateDescriptorHandle(SrvUavDescriptorSize);
	////PanelPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), SrvUavDescriptorSize, 12);
 //   PanelPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), AllocInfo);
	//PanelPtr->SetMaterialByName("Mat_Red");

    auto BoxPtr = new Box();
    BoxPtr->SetPosition(0.0f, -2.0f, 0.0f);
	BoxPtr->SetScale(10.0f, 0.5f, 10.0f);
    BoxPtr->InitVertexBufferAndIndexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());
    DescriptorAllocation AllocInfo = AllocateDescriptorHandle(SrvUavDescriptorSize);
    //PanelPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), SrvUavDescriptorSize, 12);
    BoxPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), AllocInfo);
    BoxPtr->SetMaterialByName("Mat_White");
    MeshList.push_back(BoxPtr);

    BoxPtr = new Box();
    BoxPtr->SetPosition(0.0f, -10.0f, -3.0f);
    BoxPtr->SetScale(0.5f, 15.f, 0.5f);
    BoxPtr->InitVertexBufferAndIndexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());
    AllocInfo = AllocateDescriptorHandle(SrvUavDescriptorSize);
    //PanelPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), SrvUavDescriptorSize, 12);
    BoxPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), AllocInfo);
    BoxPtr->SetMaterialByName("Mat_Red");

	MeshList.push_back(BoxPtr);


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
    RtvHeapDesc.NumDescriptors = FrameBufferCount + 1;
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
    ConstantBufferViewHeapDesc.NumDescriptors = MAX_HEAP_SIZE;
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
    auto LightCvbHandle = AllocateDescriptorHandle(SrvUavDescriptorSize);
	LightCbvCpuHandle = LightCvbHandle.CpuHandle;
	LightCbvGpuHandle = LightCvbHandle.GpuHandle;
	//LightCbvCpuHandle = ConstantBufferViewHeap->GetCPUDescriptorHandleForHeapStart();
 //   LightCbvCpuHandle.ptr += /* LightCbvHeapIndex */ 19 * SrvUavDescriptorSize;
 //   LightCbvGpuHandle = ConstantBufferViewHeap->GetGPUDescriptorHandleForHeapStart();
 //   LightCbvGpuHandle.ptr += /* LightCbvHeapIndex */ 19 * SrvUavDescriptorSize;
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

	auto MaterialCvbHandle = AllocateDescriptorHandle(SrvUavDescriptorSize);
	MaterialCbvCpuHandle = MaterialCvbHandle.CpuHandle;
	MaterialCbvGpuHandle = MaterialCvbHandle.GpuHandle;
	//MaterialCbvCpuHandle = ConstantBufferViewHeap->GetCPUDescriptorHandleForHeapStart();
	//MaterialCbvCpuHandle.ptr += MaterialCbvHeapIndex * SrvUavDescriptorSize;
	//MaterialCbvGpuHandle = ConstantBufferViewHeap->GetGPUDescriptorHandleForHeapStart();
	//MaterialCbvGpuHandle.ptr += MaterialCbvHeapIndex * SrvUavDescriptorSize;
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
    
	//t0 shader resource view for texture
    //slot 3
    rootSigBuilder.AddSRVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // t1 for shadow mask
    rootSigBuilder.AddSRVDescriptorTable(1, 1, D3D12_SHADER_VISIBILITY_PIXEL);


    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; // 视界外视为最远深度
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0; // s0
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootSigBuilder.AddStaticSampler(sampler);

    D3D12_STATIC_SAMPLER_DESC samplerShadow = {};
    samplerShadow.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; // 开启比较 + 线性滤波
    samplerShadow.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplerShadow.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplerShadow.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplerShadow.MipLODBias = 0;
    samplerShadow.MaxAnisotropy = 0;
    samplerShadow.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 小于等于则为亮
    samplerShadow.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplerShadow.MinLOD = 0.0f;
    samplerShadow.MaxLOD = D3D12_FLOAT32_MAX;
    samplerShadow.ShaderRegister = 1; // 绑定到 s1
    samplerShadow.RegisterSpace = 0;
    samplerShadow.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootSigBuilder.AddStaticSampler(samplerShadow);

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

    CD3DX12_DEPTH_STENCIL_DESC DepthDesc(D3D12_DEFAULT);
    DepthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	DepthDesc.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;

    TestPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    TestPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	TestPsoDesc.DepthStencilState = DepthDesc;
    TestPsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    TestPsoDesc.SampleMask = UINT_MAX;
    TestPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    TestPsoDesc.NumRenderTargets = 1;
    TestPsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    TestPsoDesc.SampleDesc.Count = 1;

	Material* TestMaterial = new Material("TestMaterial", RootSignature, TestPsoDesc);

    Material* MatGold = new Material("Mat_Gold", RootSignature, TestPsoDesc);
    MatGold->SetConstantData({
        {1.0f, 0.76f, 0.33f, 1.0f}, // Albedo (金黄色)
        0.2f,                       // Roughness (光滑)
        1.0f,                       // Metallic (金属)
        1.0f, 0.0f                  // AO, Padding
        });
    Material* MatRedPlastic = new Material("Mat_Red", RootSignature, TestPsoDesc);
    MatRedPlastic->SetConstantData({
        {1.0f, 0.1f, 0.1f, 1.0f},   // Albedo (红)
        0.5f,                       // Roughness (中等粗糙)
        0.0f,                       // Metallic (非金属)
        1.0f, 0.0f
        });
    Material* MatWhitePlastic = new Material("Mat_White", RootSignature, TestPsoDesc);
    MatWhitePlastic->SetConstantData({
        {0.8f, 0.8f, 0.8f, 1.0f},   // Albedo (红)
        0.5f,                       // Roughness (中等粗糙)
        0.0f,                       // Metallic (非金属)
        1.0f, 0.0f
        });

}

void DXRender::CreateFence()
{
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));
}

void DXRender::Draw()
{
    ThrowIfFailed(CommandAllocator->Reset());
    ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));
	Material& TestMaterial = MaterialManager::GetInstance().GetOrCreateMaterial("TestMaterial");
    ZPrePass.Execute(CommandList.Get());
	ShadowPass.Execute(CommandList.Get());
    ShadowMaskPass.Execute(CommandList.Get());
    MainPass.Execute(CommandList.Get());

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
		// light size for PCSS
        ImGui::SliderFloat("Light Size (PCSS)", &LightConstantInstance.LightSize, 0.0f, 5.0f);

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


}

DXRender::~DXRender()
{
    if (PtrMesh)
    {
        delete PtrMesh;
    }
}

void DXRender::InitShadowMap()
{
	auto device = Device::GetInstance().GetD3DDevice();

	D3D12_RESOURCE_DESC TexDesc = {};
	TexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	TexDesc.Alignment = 0;
	TexDesc.Width = ShadowMapSize;
	TexDesc.Height = ShadowMapSize;
	TexDesc.DepthOrArraySize = 1;
    TexDesc.MipLevels = 1;

	TexDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.SampleDesc.Quality = 0;
	TexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	TexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE ClearValue = {};
    ClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    ClearValue.DepthStencil.Depth = 1.0f;
	ClearValue.DepthStencil.Stencil = 0;


	CD3DX12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(
        &HeapProps,
        D3D12_HEAP_FLAG_NONE,
        &TexDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,  // 初始状态改为 PSR，匹配第一帧的 barrier
        &ClearValue,
        IID_PPV_ARGS(&ShadowMap)
	));
	ShadowMap->SetName(L"Shadow Map");
    // 创建DSV
	D3D12_DESCRIPTOR_HEAP_DESC DsvHeapDesc = {};
	DsvHeapDesc.NumDescriptors = 1;
	DsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	DsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ThrowIfFailed(device->CreateDescriptorHeap(&DsvHeapDesc, IID_PPV_ARGS(&ShadowDSVHeap)));

	ShadowDSVHandle = ShadowDSVHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	device->CreateDepthStencilView(ShadowMap.Get(), &dsvDesc, ShadowDSVHandle);

	//SRV for sampling in shader
	ShadowSRVHandle = AllocateDescriptorHandle(SrvUavDescriptorSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	device->CreateShaderResourceView(ShadowMap.Get(), &srvDesc, ShadowSRVHandle.CpuHandle);

    //device->CreateShaderResourceView(ShadowMap.Get(), &srvDesc, ShadowSRVHandle.CpuHandle);
    // 4. 初始化视口 (Shadow Pass 专用)
    m_ShadowViewport = { 0.0f, 0.0f, (float)ShadowMapSize, (float)ShadowMapSize, 0.0f, 1.0f };
    m_ShadowScissorRect = { 0, 0, (LONG)ShadowMapSize, (LONG)ShadowMapSize };


}

void DXRender::InitShadowPSO()
{
	auto ShadowVS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"ShadowVS", L"ShadowMapping.hlsl", "VSMain", "vs_5_0"
    )->GetBytecode();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { StandardVertexInputLayout, _countof(StandardVertexInputLayout) };
    psoDesc.pRootSignature = RootSignature.Get(); // 复用现在的 RootSig
	psoDesc.VS = { reinterpret_cast<BYTE*>(ShadowVS->GetBufferPointer()), ShadowVS->GetBufferSize() };
    psoDesc.PS = { nullptr, 0 };

    CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
	rasterizerDesc.DepthBias = 100;
    rasterizerDesc.DepthBiasClamp = 0.0f;
    rasterizerDesc.SlopeScaledDepthBias = 1.0f;
	psoDesc.RasterizerState = rasterizerDesc;

    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // 必须匹配 InitShadowMap 里的 dsvDesc
    psoDesc.NumRenderTargets = 0; // 我们不输颜色
    psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&ShadowPSO)));
    if (ShadowPSO == nullptr) {
        MessageBox(NULL, L"ShadowPSO Create Failed!", L"Error", MB_OK);
    };
}

void DXRender::InitPasses()
{
	auto device = Device::GetInstance().GetD3DDevice();
    ShadowPass.Name = "ShadowPass";
	ShadowPass.PSO = GraphicsPSOBuilder(RootSignature.Get()).SetDepthOnly(DXGI_FORMAT_D24_UNORM_S8_UINT).Build(device);

    ShadowPass.Execute = [this](ID3D12GraphicsCommandList* CommandList)
    {
        // ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));

        // Shadow Pass(生成阴影图)
        if (ShadowPSO.Get())
        {
            CommandList->SetPipelineState(ShadowPSO.Get());
        }
        ID3D12DescriptorHeap* ShadowDescriptorHeaps[] = { ConstantBufferViewHeap.Get() };
        CommandList->SetDescriptorHeaps(_countof(ShadowDescriptorHeaps), ShadowDescriptorHeaps);
        CD3DX12_RESOURCE_BARRIER Barrier_SR2W = CD3DX12_RESOURCE_BARRIER::Transition(
            ShadowMap.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE
        );
        CommandList->ResourceBarrier(1, &Barrier_SR2W);
        CommandList->OMSetRenderTargets(0, nullptr, FALSE, &ShadowDSVHandle);
        CommandList->ClearDepthStencilView(ShadowDSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        CommandList->RSSetViewports(1, &m_ShadowViewport);
        CommandList->RSSetScissorRects(1, &m_ShadowScissorRect);

        // Shadow Pass 绘制场景到深度图
        // todo
        DirectX::XMVECTOR lightDirVec = XMLoadFloat3(&LightConstantInstance.LightDirection);
        lightDirVec = DirectX::XMVector3Normalize(lightDirVec);

        DirectX::XMVECTOR lightPos = DirectX::XMVectorScale(lightDirVec, 20.0f);
        DirectX::XMVECTOR targetPos = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f); // 看向原点
        DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(
            DirectX::XMVectorSubtract(targetPos, lightPos)
        );
        float dot = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(forward, up)));

        // 如果太接近 (比如 > 0.99)，说明视线几乎垂直于地面
        if (dot > 0.99f)
        {
            // 这种情况下，强制把 Z 轴当作 Up 向量
            up = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        }

        DirectX::XMMATRIX lightView = DirectX::XMMatrixLookAtLH(lightPos, targetPos, up);

        // 正交投影范围 (覆盖你的场景大小)
        // 比如场景是 20x20 米，这里就设宽一点
        DirectX::XMMATRIX lightProj = DirectX::XMMatrixOrthographicLH(20.0f, 20.0f, 1.0f, 50.0f); // 宽, 高, 近, 远
        DirectX::XMMATRIX lightViewProj = lightView * lightProj;

        CommandList->SetGraphicsRootSignature(RootSignature.Get());
        auto MainCameraPos = MainCamera.GetPosition();
        LightConstantInstance.CameraPosition = { MainCameraPos.x, MainCameraPos.y,MainCameraPos.z };

        // 将数据拷贝到 Map 好的内存中
        if (LightConstantBufferMappedData)
        {
            memcpy(LightConstantBufferMappedData, &LightConstantInstance, sizeof(LightConstants));
        }
        CommandList->SetGraphicsRootDescriptorTable(1, LightCbvGpuHandle);

        // XMStoreFloat4x4(&shadowConstants.WorldViewProj, XMMatrixTranspose(MVP));
        XMStoreFloat4x4(&LightConstantInstance.LightViewProj, XMMatrixTranspose(lightViewProj));

        for (auto MeshElement : MeshList)
        {
            // 计算 MVP = World * LightView * LightProj
            // 更新常量缓冲区
            auto MVPMatrix = MeshElement->CalMVPMatrix(MainCamera.CalViewProjMatrix());
            auto M_Matrix = MeshElement->GetWorldMatrix();
            ObjectConstants objConstants;
            DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
            DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(M_Matrix));
            MeshElement->UpdateObjectConstantBuffer(objConstants);
            // 绑定
            CommandList->SetGraphicsRootDescriptorTable(0, MeshElement->GetCbvGpuHandle());
            // 绘制
            CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            auto VBView = MeshElement->GetVertexBufferView();
            CommandList->IASetVertexBuffers(0, 1, &VBView);
            auto IBView = MeshElement->GetIndexBufferView();
            CommandList->IASetIndexBuffer(&IBView);
            CommandList->DrawIndexedInstanced(MeshElement->GetIndexCount(), 1, 0, 0, 0);
        }

        // Shadow pass 完成后，将 shadow map 从 depth write 转为 shader resource
        CD3DX12_RESOURCE_BARRIER Barrier_W2PSR = CD3DX12_RESOURCE_BARRIER::Transition(
            ShadowMap.Get(),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        CommandList->ResourceBarrier(1, &Barrier_W2PSR);

        // 立即执行 shadow pass，避免 CB 被主渲染 pass 覆盖
        //ExecuteCommandAndWaitForComplete();

        // 重置 command list 准备主渲染 pass
        //ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));
	};

    MainPass.Name = "Main Pass";
    MainPass.Execute = [this](ID3D12GraphicsCommandList* CommandList)
    {
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
        // CommandList->ClearDepthStencilView(CPU_DSV_Handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        CommandList->OMSetRenderTargets(1, &CPU_RTV_Handle, FALSE, &CPU_DSV_Handle);
        //CommandList->OMSetRenderTargets(1, &CPU_RTV_Handle, FALSE, nullptr);

        CommandList->RSSetViewports(1, &ScreenViewport);
        CommandList->RSSetScissorRects(1, &ScissorRect);

        CommandList->SetGraphicsRootSignature(RootSignature.Get());

        // 绑定描述符堆
        ID3D12DescriptorHeap* descriptorHeaps[] = { ConstantBufferViewHeap.Get() };
        CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

        // 设置主渲染 PSO
        //CommandList->SetPipelineState(TestMaterial.GetPSO().Get());

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

		// shadow map SRV
        CommandList->SetGraphicsRootDescriptorTable(3, ShadowSRVHandle.GpuHandle);
        // shadow mask SRV
		CommandList->SetGraphicsRootDescriptorTable(4, ShadowMaskSRVHandle.GpuHandle);

        for (auto MeshElement : MeshList)
        {
            {
                // 更新常量缓冲区
                auto MVPMatrix = MeshElement->CalMVPMatrix(MainCamera.CalViewProjMatrix());
                auto M_Matrix = MeshElement->GetWorldMatrix();
                ObjectConstants objConstants;
                DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
                DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(M_Matrix));
                MeshElement->UpdateObjectConstantBuffer(objConstants);

                auto MaterialName = MeshElement->GetMaterialName();
                Material* MaterialPtr = MaterialManager::GetInstance().GetMaterialByName(MaterialName);
                if (MaterialPtr)
                {
                    MaterialPtr->Bind(CommandList);
                    //std::cout << "Binding Material: " << MaterialName << std::endl;

                }
            }

            // 绑定 CBV
            // CommandList->SetGraphicsRootDescriptorTable(0, ConstantBufferViewHeap->GetGPUDescriptorHandleForHeapStart());
            CommandList->SetGraphicsRootDescriptorTable(0, MeshElement->GetCbvGpuHandle());
            CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            auto VertexBufferView = MeshElement->GetVertexBufferView();
            CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
            auto IndexBufferView = MeshElement->GetIndexBufferView();
            CommandList->IASetIndexBuffer(&IndexBufferView);

            CommandList->DrawIndexedInstanced(MeshElement->GetIndexCount(), 1, 0, 0, 0);
        }
        // MainPass 不执行，等 ImGui 渲染完后统一执行
	};

    ShadowMaskPass.Name = "ShadowMaskPass";
    ShadowMaskPass.Execute = [&](ID3D12GraphicsCommandList* CommandList)
        {
            CD3DX12_RESOURCE_BARRIER Barrier_P2RT = CD3DX12_RESOURCE_BARRIER::Transition(
                ShadowMaskTexture.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET
            );
            CommandList->ResourceBarrier(1, &Barrier_P2RT);

            float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
            CommandList->ClearRenderTargetView(ShadowMaskRTVHandle, clearColor, 0, nullptr);

            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DsvHeap->GetCPUDescriptorHandleForHeapStart();
            CommandList->OMSetRenderTargets(1, &ShadowMaskRTVHandle, FALSE, &dsvHandle);
            CommandList->RSSetViewports(1, &ScreenViewport);
            CommandList->RSSetScissorRects(1, &ScissorRect);

            // 绑定描述符堆
            ID3D12DescriptorHeap* descriptorHeaps[] = { ConstantBufferViewHeap.Get() };
            CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

            CommandList->SetGraphicsRootSignature(RootSignature.Get());
            CommandList->SetGraphicsRootDescriptorTable(3, ShadowSRVHandle.GpuHandle); // 绑定 shadow map SRV
			CommandList->SetPipelineState(ShadowMaskPass.PSO.Get());

            //Light Constants
            auto MainCameraPos = MainCamera.GetPosition();
            LightConstantInstance.CameraPosition = { MainCameraPos.x, MainCameraPos.y,MainCameraPos.z };

            if (LightConstantBufferMappedData)
            {
                memcpy(LightConstantBufferMappedData, &LightConstantInstance, sizeof(LightConstants));
            }
            CommandList->SetGraphicsRootDescriptorTable(1, LightCbvGpuHandle);

            for (auto MeshElement : MeshList)
            {
                {
                    // 更新常量缓冲区
                    auto MVPMatrix = MeshElement->CalMVPMatrix(MainCamera.CalViewProjMatrix());
                    auto M_Matrix = MeshElement->GetWorldMatrix();
                    ObjectConstants objConstants;
                    DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
                    DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(M_Matrix));
                    MeshElement->UpdateObjectConstantBuffer(objConstants);

                }

                // 绑定 CBV
                // CommandList->SetGraphicsRootDescriptorTable(0, ConstantBufferViewHeap->GetGPUDescriptorHandleForHeapStart());
                CommandList->SetGraphicsRootDescriptorTable(0, MeshElement->GetCbvGpuHandle());
                CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                auto VertexBufferView = MeshElement->GetVertexBufferView();
                CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
                auto IndexBufferView = MeshElement->GetIndexBufferView();
                CommandList->IASetIndexBuffer(&IndexBufferView);

                CommandList->DrawIndexedInstanced(MeshElement->GetIndexCount(), 1, 0, 0, 0);
            }
            CD3DX12_RESOURCE_BARRIER Barrier_RT2P = CD3DX12_RESOURCE_BARRIER::Transition(
                ShadowMaskTexture.Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            );
            CommandList->ResourceBarrier(1, &Barrier_RT2P);

        };

    ZPrePass.Name = "ZPrepass";

    ZPrePass.Execute = [this](ID3D12GraphicsCommandList* CommandList)
        {
            // ZPrepass implementation (similar to ShadowPass but for main camera)
            // You can fill this in as needed
                        // 绑定描述符堆
            ID3D12DescriptorHeap* descriptorHeaps[] = { ConstantBufferViewHeap.Get() };
            CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle = DsvHeap->GetCPUDescriptorHandleForHeapStart();
			CommandList->ClearDepthStencilView(DsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			CommandList->OMSetRenderTargets(0, nullptr, FALSE, &DsvHandle);

            CommandList->RSSetViewports(1, &ScreenViewport);
            CommandList->RSSetScissorRects(1, &ScissorRect);

			CommandList->SetGraphicsRootSignature(RootSignature.Get());
			CommandList->SetPipelineState(ZPrePass.PSO.Get());

            for (auto MeshElement : MeshList)
            {

                // 更新常量缓冲区
                auto MVPMatrix = MeshElement->CalMVPMatrix(MainCamera.CalViewProjMatrix());
                auto M_Matrix = MeshElement->GetWorldMatrix();
                ObjectConstants objConstants;
                DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
                DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(M_Matrix));
                MeshElement->UpdateObjectConstantBuffer(objConstants);

                // cbv
                CommandList->SetGraphicsRootDescriptorTable(0, MeshElement->GetCbvGpuHandle());
                CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                auto VertexBufferView = MeshElement->GetVertexBufferView();
                CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
                auto IndexBufferView = MeshElement->GetIndexBufferView();
                CommandList->IASetIndexBuffer(&IndexBufferView);

                CommandList->DrawIndexedInstanced(MeshElement->GetIndexCount(), 1, 0, 0, 0);

            }
	    };

}

void DXRender::InitShadowMaskTexture()
{
    auto device = Device::GetInstance().GetD3DDevice();
    D3D12_RESOURCE_DESC TexDesc = {};
	TexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    TexDesc.Width = Width;
    TexDesc.Height = Height;

	TexDesc.DepthOrArraySize = 1;
	TexDesc.MipLevels = 1;
	TexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	float ClearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

	CD3DX12_CLEAR_VALUE ClearValue(DXGI_FORMAT_R8G8B8A8_UNORM, ClearColor);

    CD3DX12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(
        &HeapProps,
        D3D12_HEAP_FLAG_NONE,
        &TexDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,  // 初始状态改为 PSR，匹配第一帧的 barrier
        &ClearValue,
        IID_PPV_ARGS(&ShadowMaskTexture)
    ));


    ShadowMaskTexture->SetName(L"Shadow Mask Tetxure");


	ShadowMaskRTVHandle = RtvHeap->GetCPUDescriptorHandleForHeapStart();
    ShadowMaskRTVHandle.Offset(FrameBufferCount, RtvDescriptorSize);

	device->CreateRenderTargetView(ShadowMaskTexture.Get(), nullptr, ShadowMaskRTVHandle);
    ShadowMaskSRVHandle = AllocateDescriptorHandle(SrvUavDescriptorSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    Device::GetInstance().GetD3DDevice()->CreateShaderResourceView(
        ShadowMaskTexture.Get(), &srvDesc, ShadowMaskSRVHandle.CpuHandle);
}

void DXRender::InitShadowMaskPSO()
{
    auto ShadowMaskVS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"ShadowMaskVS", L"ShadowMaskShader.hlsl", "VSMain", "vs_5_0"
    )->GetBytecode();
    auto ShadowMaskPS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"ShadowMaskPS", L"ShadowMaskShader.hlsl", "PSMain", "ps_5_0"
	)->GetBytecode();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { StandardVertexInputLayout, _countof(StandardVertexInputLayout) };
    psoDesc.pRootSignature = RootSignature.Get(); // 复用现在的 RootSig
    psoDesc.VS = { reinterpret_cast<BYTE*>(ShadowMaskVS->GetBufferPointer()), ShadowMaskVS->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(ShadowMaskPS->GetBufferPointer()), ShadowMaskPS->GetBufferSize() };

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    //PsoDesc.DepthStencilState.DepthEnable = TRUE;
    // 1. 获取默认设置
    CD3DX12_DEPTH_STENCIL_DESC depthDesc(D3D12_DEFAULT);

    // 2. 改为 "小于等于" (LESS_EQUAL)，这样相同深度的像素也能通过测试
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // 3. 关闭深度写入 (MainPass 已经写过深度了，这里只读就行，优化性能)
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    psoDesc.DepthStencilState = depthDesc;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&ShadowMaskPass.PSO)));

}

void DXRender::InitZPrepassPSO()
{
    auto VsByte = DXShaderManager::GetInstance().CreateOrFindShader(
        L"ZPrepassVS", L"ZPrepassShader.hlsl", "VSMain", "vs_5_0"
	)->GetBytecode();
    auto PsByte = DXShaderManager::GetInstance().CreateOrFindShader(
		L"ZPrepassPS", L"ZPrepassShader.hlsl", "PSMain", "ps_5_0"
	)->GetBytecode();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { StandardVertexInputLayout, _countof(StandardVertexInputLayout) };
    psoDesc.pRootSignature = RootSignature.Get(); // 复用现在的 RootSig
	psoDesc.VS = { reinterpret_cast<BYTE*>(VsByte->GetBufferPointer()), VsByte->GetBufferSize() };
	psoDesc.PS = { reinterpret_cast<BYTE*>(PsByte->GetBufferPointer()), PsByte->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // 必须匹配 InitDepthStencilBuffer 里的 dsvDesc
    psoDesc.NumRenderTargets = 0; // 我们不输颜色
    psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&ZPrePass.PSO)));
}

DXRender::DXRender()
{
    MainCamera.Init((float)Width, (float)Height);
}

DescriptorAllocation DXRender::AllocateDescriptorHandle(unsigned int DescriptorSize)
{
    if(CurrentSrvHeapIndex >= MAX_HEAP_SIZE)
    {
        throw std::runtime_error("Exceeded maximum descriptor heap size!");
	}
    // 计算 CPU 句柄 (用于创建资源)
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(
        ConstantBufferViewHeap->GetCPUDescriptorHandleForHeapStart(),
        CurrentSrvHeapIndex,
        DescriptorSize);

    // 计算 GPU 句柄 (用于绑定 Shader)
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(
        ConstantBufferViewHeap->GetGPUDescriptorHandleForHeapStart(),
        CurrentSrvHeapIndex,
        DescriptorSize);

	unsigned int RetIdx = CurrentSrvHeapIndex;

    CurrentSrvHeapIndex++; // 指针后移

    return { cpuHandle, gpuHandle, RetIdx };
    //return std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>();
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

GraphicsPSOBuilder& GraphicsPSOBuilder::SetShaders(const std::wstring& vsName, const std::wstring& psName)
{
    auto vs = DXShaderManager::GetInstance().GetShaderByName(vsName)->GetBytecode();

    m_Desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };

    if (!psName.empty()) {
        auto ps = DXShaderManager::GetInstance().GetShaderByName(psName)->GetBytecode();
        m_Desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    }
    else {
        m_Desc.PS = { nullptr, 0 };
    }
    return *this;
}

void Texture::LoadFromFile(ID3D12GraphicsCommandList* CmdList, std::string Filename, bool isRGB)
{

    // texture->cpu->gpu and destory on cpu
	auto device = Device::GetInstance().GetD3DDevice();


    int NrComponent;
	unsigned char* Data = stbi_load(
		Filename.c_str(),
		&Width,
		&Height,
		&NrComponent,
		4
	);

    if (!Data)
    {
        return;
    }
	Format = isRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;

    CreateTextureResource(device, CmdList, Data, Width, Height, Format, 4, Resource, UploadHeap);
	stbi_image_free(Data);

    auto size = DXRender::GetInstance().GetSrvUavDescriptorSize();
    Handle = DXRender::GetInstance().AllocateDescriptorHandle(size);

    // SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SrvDesc.Format = Format;
	SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView(Resource.Get(), &SrvDesc, Handle.CpuHandle);

}

void Texture::LoadHDRFromFile(ID3D12GraphicsCommandList* CmdList, std::string Filename)
{
    // texture->cpu->gpu and destory on cpu
    auto device = Device::GetInstance().GetD3DDevice();


    int NrComponent;
    float* Data = stbi_loadf(
        Filename.c_str(),
        &Width,
        &Height,
        &NrComponent,
        4
    );

    if (!Data)
    {
        return;
    }
    Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    CreateTextureResource(device, CmdList, Data, Width, Height, Format, 16, Resource, UploadHeap);
    stbi_image_free(Data);

    auto size = DXRender::GetInstance().GetSrvUavDescriptorSize();
    Handle = DXRender::GetInstance().AllocateDescriptorHandle(size);

    // SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SrvDesc.Format = Format;
    SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView(Resource.Get(), &SrvDesc, Handle.CpuHandle);

}

void Texture::Release()
{
    Resource.Reset();
    UploadHeap.Reset();

    Width = 0;
    Height = 0;
}

void Texture::CreateTextureResource(ID3D12Device* device, ID3D12GraphicsCommandList* CmdList, const void* InData, int Width, int Height, DXGI_FORMAT Format, int PixelByteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& OutResource, Microsoft::WRL::ComPtr<ID3D12Resource>& OutUploadHeap)
{
    D3D12_RESOURCE_DESC Desc = {};
    Desc.MipLevels = 1;
	Desc.Format = Format;
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	Desc.DepthOrArraySize = 1;
	Desc.SampleDesc.Count = 1;
    Desc.SampleDesc.Quality = 0;
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	CD3DX12_HEAP_PROPERTIES DefaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&DefaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&Desc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&OutResource)
	));
	OutResource->SetName(L"Texture Resource");
	const UINT64 UploadBufferSize = GetRequiredIntermediateSize(OutResource.Get(), 0, 1);
	CD3DX12_HEAP_PROPERTIES UploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(UploadBufferSize);
	ThrowIfFailed(device->CreateCommittedResource(
		&UploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&BufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&OutUploadHeap)
	));
	OutUploadHeap->SetName(L"Texture Upload Heap");
	D3D12_SUBRESOURCE_DATA TextureData = {};
	TextureData.pData = InData;
	TextureData.RowPitch = Width * PixelByteSize;
	TextureData.SlicePitch = TextureData.RowPitch * Height;
	UpdateSubresources(CmdList, OutResource.Get(), OutUploadHeap.Get(), 0, 0, 1, &TextureData);
	CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		OutResource.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
	CmdList->ResourceBarrier(1, &Barrier);
}
