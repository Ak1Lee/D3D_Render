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


#include "Model.h"

//tex load
// #define STB_IMAGE_IMPLEMENTATION
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
	InitSkyPassPSO();
	InitPasses();
	
    ThrowIfFailed(CommandAllocator->Reset());
    ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));
    
    // InitEnvCubeMap 需要 CommandList 处于打开状态
    InitEnvCubeMapAndIrradianceMap();
    InitPrefilterRootSignature();
    InitBRDFLUT();


	//HDRTex.LoadHDRFromFile(CommandList.Get(), ".\\resources\\puresky_2k.hdr");
	m_HDRSkyTexture = std::make_shared<Texture>("HDRSky");
	m_HDRSkyTexture->LoadFromFile(Device::GetInstance().GetD3DDevice(),CommandList.Get(), ".\\resources\\venice_sunset_1k.hdr",false,true);

    ExecuteCommandAndWaitForComplete();

    

    // imgui - 在 InitDX 之后初始化
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1; // ImGui 只需要一个位置存字体贴图
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // 必须是 Shader 可见的
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&ImguiSrvHeap)));

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


    ThrowIfFailed(CommandAllocator->Reset());
    ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));

    ComputeCubemap();
    ComputeIrradianceMap();
    ComputePrefilterMap();
    ComputeBRDFLUT();

    ExecuteCommandAndWaitForComplete();

    PreDraw();

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

    // add descriptor manager
    DescriptorAllocatorManager::GetInstance().Init(Device::GetInstance().GetD3DDevice());


    InitRootSignature();

    InitComputeRootSignature();
    InitIrradianceMapCompute();

    CompileShader();



    CreateFence();
    ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));

    CreateConstantBufferView();


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
		DescriptorHandle AllocInfo = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
        // BoxPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), AllocInfo);
		BoxPtr->SetMaterialByName("Mat_Red");
        MeshList.push_back(BoxPtr);
    }
    // 默认 1x1 白色贴图
    {
        uint32_t whitePixel = 0xFFFFFFFF;
        m_DefaultWhiteTexture = std::make_shared<Texture>("DefaultWhite");
        m_DefaultWhiteTexture->Width = 1;
        m_DefaultWhiteTexture->Height = 1;
        m_DefaultWhiteTexture->Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        m_DefaultWhiteTexture->CreateResourceHeap(
            Device::GetInstance().GetD3DDevice(), CommandList.Get(), &whitePixel, 4);
        m_DefaultWhiteTexture->ViewFlags = TextureViewFlags::SRV;
        m_DefaultWhiteTexture->CreateSRV(Device::GetInstance().GetD3DDevice());
    }

    // test model
    auto backpack = new Model();
    backpack->LoadFromFile(".\\resources\\backpack\\backpack.obj",
        Device::GetInstance().GetD3DDevice(), CommandList.Get());
    backpack->SetPosition(0.0f, 5.0f, 5.0f);
    backpack->SetMaterialByName("Scene_-_Root");
    MeshList.push_back(backpack);

    int rows = 7;
    int cols = 7;
    float spacing = 2.5f;

    for (int y = 0; y < rows; ++y)
    {
        // 计算金属度 (0.0 -> 1.0)
        float metallic = (float)y / (float)(rows - 1);

        for (int x = 0; x < cols; ++x)
        {
            // 计算粗糙度 (0.05 -> 1.0)
            // Clamp 到 0.05 是为了防止除0错误导致亮点闪烁
            float roughness = (std::max)((float)x / (float)(cols - 1), 0.05f);

            // 1. 【核心修改】动态创建一个独一无二的材质
            // 给它起个唯一的名字，比如 "Mat_Test_0_0", "Mat_Test_0_1"
            std::string matName = "Mat_Test_" + std::to_string(x) + "_" + std::to_string(y);

            auto tempMat = std::make_shared<Material>(matName);
            tempMat->SetConstantData({
                {1.0f, 0.0f, 0.0f, 1.0f},
                roughness, metallic, 1.0f, 0.0f
                });
            MaterialManager::GetInstance().AddMaterial(tempMat);

            // 3. 如果你的 MaterialManager 需要注册，记得注册进去
            // MaterialManager::GetInstance().AddMaterial(tempMat); 
            // 假设你目前是自己管理的，确保不要内存泄漏即可

            // 4. 创建球体
            auto SpherePtr = new Sphere();

            // 居中排列位置
            float posX = (x - (cols / 2)) * spacing;
            float posY = (y - (rows / 2)) * spacing + 10;
            SpherePtr->SetPosition(posX, posY, 0.0f);

            SpherePtr->InitVertexBufferAndIndexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());

            // DescriptorHandle AllocInfo = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
            // SpherePtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), AllocInfo);

            // 5. 绑定刚才创建的材质
            // 如果你的 Sphere 支持直接传指针：
            // SpherePtr->SetMaterial(tempMat);

            // 或者如果必须传名字：
            SpherePtr->SetMaterialByName(matName);

            MeshList.push_back(SpherePtr);
        }
    }


    auto BoxPtr = new Box();
    BoxPtr->SetPosition(0.0f, -2.0f, 0.0f);
	BoxPtr->SetScale(10.0f, 0.5f, 10.0f);
    BoxPtr->InitVertexBufferAndIndexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());
    // DescriptorHandle AllocInfo = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
    // PanelPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), SrvUavDescriptorSize, 12);
    // BoxPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), AllocInfo);
    BoxPtr->SetMaterialByName("Mat_White");
    MeshList.push_back(BoxPtr);

    BoxPtr = new Box();
    BoxPtr->SetPosition(0.0f, -10.0f, -3.0f);
    BoxPtr->SetScale(0.5f, 15.f, 0.5f);
    BoxPtr->InitVertexBufferAndIndexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());
    // AllocInfo = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
    // PanelPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), ConstantBufferViewHeap.Get(), SrvUavDescriptorSize, 12);
    // BoxPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), AllocInfo);
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

    // Create For Frame Resource
    for (int i = 0; i < FrameResourceNum; ++i)
    {
		FrameResources[i].Init(Device::GetInstance().GetD3DDevice(),100);
    }
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
    //D3D12_DESCRIPTOR_HEAP_DESC ConstantBufferViewHeapDesc = {};
    //ConstantBufferViewHeapDesc.NumDescriptors = MAX_HEAP_SIZE;
    //ConstantBufferViewHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    //ConstantBufferViewHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    //ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateDescriptorHeap(&ConstantBufferViewHeapDesc, IID_PPV_ARGS(&ConstantBufferViewHeap)));

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
    auto LightCvbHandle = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
	LightCbvCpuHandle = LightCvbHandle.CpuHandle;
	LightCbvGpuHandle = LightCvbHandle.GpuHandle;
    // 创建 CBV 
    D3D12_CONSTANT_BUFFER_VIEW_DESC LightCbvDesc = {};
    LightCbvDesc.BufferLocation = LightConstantBuffer->GetGPUVirtualAddress();
    LightCbvDesc.SizeInBytes = LightConstantBufferSize;
	Device::GetInstance().GetD3DDevice()->CreateConstantBufferView(&LightCbvDesc, LightCbvCpuHandle);


	// Material Constant Buffer
 //   const UINT MaterialConstantBufferSize = (sizeof(MaterialConstants) + 255) & ~255;
 //   CD3DX12_HEAP_PROPERTIES MatHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	//CD3DX12_RESOURCE_DESC MatBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(MaterialConstantBufferSize);
	//ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateCommittedResource(&MatHeapProps, D3D12_HEAP_FLAG_NONE, &MatBufferDesc,
	//	D3D12_RESOURCE_STATE_GENERIC_READ,
	//	nullptr,
	//	IID_PPV_ARGS(&MaterialConstantBuffer)));
 //   CD3DX12_RANGE MatReadRange(0, 0);
 //   ThrowIfFailed(MaterialConstantBuffer->Map(0, &MatReadRange, reinterpret_cast<void**>(&MaterialConstantBufferMappedData)));

	//auto MaterialCvbHandle = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
	//MaterialCbvCpuHandle = MaterialCvbHandle.CpuHandle;
	//MaterialCbvGpuHandle = MaterialCvbHandle.GpuHandle;
	//D3D12_CONSTANT_BUFFER_VIEW_DESC MaterialCbvDesc = {};
	//MaterialCbvDesc.BufferLocation = MaterialConstantBuffer->GetGPUVirtualAddress();
	//MaterialCbvDesc.SizeInBytes = MaterialConstantBufferSize;
 //   Device::GetInstance().GetD3DDevice()->CreateConstantBufferView(&MaterialCbvDesc, MaterialCbvCpuHandle);


}

void DXRender::InitRootSignature()
{
    // 根参数 —— index 就是添加顺序
	DXRootSignature rootSigBuilder;
    // Index 0: b0 Object CB (Root CBV)
    rootSigBuilder.AddRootConstantBufferView(0);
    // Index 1: b1 Light CB (Root CBV)
    rootSigBuilder.AddRootConstantBufferView(1);
    // Index 2: b2 Material CB (Descriptor Table)
    rootSigBuilder.AddRootConstantBufferView(2);
    
	//t0 shader resource view for texture
    //slot 3
    rootSigBuilder.AddSRVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // t1 for shadow mask
    rootSigBuilder.AddSRVDescriptorTable(1, 1, D3D12_SHADER_VISIBILITY_PIXEL);

    // ibl
    // --- IBL 贴图表 (Index 5) ---
    // t10, t11, t12
    // Index 5: t10 (Irradiance)
    rootSigBuilder.AddSRVDescriptorTable(10, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // Index 6: t11 (Prefilter)
    rootSigBuilder.AddSRVDescriptorTable(11, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // Index 7: t12 (BRDF LUT)
    rootSigBuilder.AddSRVDescriptorTable(12, 1, D3D12_SHADER_VISIBILITY_PIXEL);

    // pbr tetxure
	// --- PBR 贴图表 (Index 8) abedo ---
    rootSigBuilder.AddSRVDescriptorTable(13, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // --- PBR 贴图表 (Index 9) normal---
    rootSigBuilder.AddSRVDescriptorTable(14, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // --- PBR 贴图表 (Index 10) metallic---
    rootSigBuilder.AddSRVDescriptorTable(15, 1, D3D12_SHADER_VISIBILITY_PIXEL);


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

    D3D12_STATIC_SAMPLER_DESC samplerLinear = {};
    samplerLinear.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 线性
    samplerLinear.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerLinear.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerLinear.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerLinear.MipLODBias = 0;
    samplerLinear.MaxAnisotropy = 16;
    samplerLinear.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerLinear.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplerLinear.MinLOD = 0.0f;
    samplerLinear.MaxLOD = D3D12_FLOAT32_MAX;
    samplerLinear.ShaderRegister = 2; // <--- 关键：绑定到 s2
    samplerLinear.RegisterSpace = 0;
    samplerLinear.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootSigBuilder.AddStaticSampler(samplerLinear);

    //IBL Linear + Clamp
    D3D12_STATIC_SAMPLER_DESC samplerIBL = {};
    samplerIBL.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 必须是线性
    samplerIBL.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // 【关键】Clamp
    samplerIBL.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // 【关键】Clamp
    samplerIBL.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerIBL.MipLODBias = 0;
    samplerIBL.MaxAnisotropy = 16;
    samplerIBL.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerIBL.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplerIBL.MinLOD = 0.0f;
    samplerIBL.MaxLOD = D3D12_FLOAT32_MAX;
    samplerIBL.ShaderRegister = 3; // 绑定到 s3
    samplerIBL.RegisterSpace = 0;
    samplerIBL.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootSigBuilder.AddStaticSampler(samplerIBL);

    RootSignature = rootSigBuilder.Build(Device::GetInstance().GetD3DDevice());

}
void DXRender::InitComputeRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0
    CD3DX12_DESCRIPTOR_RANGE uavRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0

    DXRootSignature rootSigBuilder;
    // t0
	rootSigBuilder.AddSRVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_ALL);
    // u0 CubeMap Output
    rootSigBuilder.AddUAVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_ALL);

    D3D12_STATIC_SAMPLER_DESC LinearSampler = {};
	LinearSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	LinearSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	LinearSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    LinearSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

	LinearSampler.MipLODBias = 0;
	LinearSampler.MaxAnisotropy = 16;
	LinearSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    LinearSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;

	LinearSampler.MinLOD = 0.0f;
	LinearSampler.MaxLOD = D3D12_FLOAT32_MAX;
	LinearSampler.ShaderRegister = 0; // s0
	LinearSampler.RegisterSpace = 0;
	LinearSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;


    rootSigBuilder.AddStaticSampler(LinearSampler);

	ComputeRootSignature = rootSigBuilder.Build(Device::GetInstance().GetD3DDevice());


    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = ComputeRootSignature.Get();
	auto CSShader = DXShaderManager::GetInstance().CreateOrFindShader(L"TestCS", L"Equirect2Cube.hlsl", "CSMain", "cs_5_0");
	psoDesc.CS = { reinterpret_cast<BYTE*>(CSShader->GetBytecode()->GetBufferPointer()), CSShader->GetBytecode()->GetBufferSize() };


    HRESULT hr = Device::GetInstance().GetD3DDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&ComputePipelineState));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create Compute PSO");
    }
}

void DXRender::ComputeCubemap()
{
	CommandList->SetPipelineState(ComputePipelineState.Get());
	CommandList->SetComputeRootSignature(ComputeRootSignature.Get());
    ID3D12DescriptorHeap* ppHeaps[] = { 
        DescriptorAllocatorManager::GetInstance().GetCBV_SRV_UAV_Heap()
    };
    CommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	m_EnvCubeMap->TransitionTo(CommandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	// 设置描述符堆
	//CommandList->SetComputeRootDescriptorTable(0, m_HDRSkyTexture->GetSRV_G());
    m_HDRSkyTexture->BindSRV_Compute(CommandList.Get(), 0);
	// CommandList->SetComputeRootDescriptorTable(1, m_EnvCubeMap->GetUAV_G());
    m_EnvCubeMap->BindUAV_Compute(CommandList.Get(), 1);

	CommandList->Dispatch(1024 / 32, 1024 / 32, 6); // 每个线程组处理16x16像素

	m_EnvCubeMap->TransitionTo(CommandList.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    // ExecuteCommandAndWaitForComplete();
}

void DXRender::ComputeIrradianceMap()
{
	CommandList->SetPipelineState(ComputeIrraPipelineState.Get());
	CommandList->SetComputeRootSignature(ComputeIrraRootSignature.Get());
    ID3D12DescriptorHeap* ppHeaps[] = {
        DescriptorAllocatorManager::GetInstance().GetCBV_SRV_UAV_Heap()
    };
    CommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	m_EnvCubeMap->BindSRV_Compute(CommandList.Get(), 0);
	m_IrradianceMap->BindUAV_Compute(CommandList.Get(), 1);
    CommandList->Dispatch(4, 4, 6);
}


void DXRender::InitIrradianceMapCompute()
{
    m_IrradianceMap = std::make_shared<Texture>("IrradianceMap");
    auto cubeDesc = TextureDesc::CreateCube(
        128,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        TextureViewFlags::SRV | TextureViewFlags::UAV
    );
    m_IrradianceMap->Create(CommandList.Get(), cubeDesc);

    m_PrefilterMap = std::make_shared<Texture>("PrefilterMap");
    auto cubeMipDesc = TextureDesc::CreateCube(
        128,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        TextureViewFlags::SRV | TextureViewFlags::UAV
    );
    cubeMipDesc.Width = 128; // 起始大小
    cubeMipDesc.Height = 128;
    cubeMipDesc.MipLevels = 5; // 128, 64, 32, 16, 8
    cubeMipDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    cubeMipDesc.ViewFlags = TextureViewFlags::SRV | TextureViewFlags::UAV;

    m_PrefilterMap->Create(CommandList.Get(), cubeMipDesc);


    CD3DX12_DESCRIPTOR_RANGE srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0
    CD3DX12_DESCRIPTOR_RANGE uavRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0

    DXRootSignature rootSigBuilder;
    // t0 m_EnvCubeMap
    rootSigBuilder.AddSRVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_ALL);
    // u0 IrradianceMap Output
    rootSigBuilder.AddUAVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_ALL);

    D3D12_STATIC_SAMPLER_DESC LinearSampler = {};
    LinearSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    LinearSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    LinearSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    LinearSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

    LinearSampler.MipLODBias = 0;
    LinearSampler.MaxAnisotropy = 16;
    LinearSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    LinearSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;

    LinearSampler.MinLOD = 0.0f;
    LinearSampler.MaxLOD = D3D12_FLOAT32_MAX;
    LinearSampler.ShaderRegister = 0; // s0
    LinearSampler.RegisterSpace = 0;
    LinearSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;


    rootSigBuilder.AddStaticSampler(LinearSampler);

    ComputeIrraRootSignature = rootSigBuilder.Build(Device::GetInstance().GetD3DDevice());


    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = ComputeIrraRootSignature.Get();
    auto CSShader = DXShaderManager::GetInstance().CreateOrFindShader(L"IrradianceMapComputeShader", L"IrradianceMapCompute.hlsl", "CSMain", "cs_5_0");
    psoDesc.CS = { reinterpret_cast<BYTE*>(CSShader->GetBytecode()->GetBufferPointer()), CSShader->GetBytecode()->GetBufferSize() };
    HRESULT hr = Device::GetInstance().GetD3DDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&ComputeIrraPipelineState));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create Compute PSO");
    }
}


void DXRender::InitEnvCubeMapAndIrradianceMap()
{

	m_EnvCubeMap = std::make_shared<Texture>();
    auto cubeDesc = TextureDesc::CreateCube(
        1024,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        TextureViewFlags::SRV | TextureViewFlags::UAV
    );
    m_EnvCubeMap->Create(CommandList.Get(), cubeDesc);

	//m_IrradianceMap = std::make_shared<Texture>();


    // sky box
	SkyboxMesh = new Box();
	SkyboxMesh->SetScale(1.0f, 1.0f, 1.0f);
    SkyboxMesh->InitVertexBufferAndIndexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());
    // DescriptorHandle AllocInfo = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
    // SkyboxMesh->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), AllocInfo);



}

void DXRender::InitIrradianceMap()
{
}

void DXRender::InitPrefilterRootSignature()
{
	DXRootSignature rootSigBuilder;
	// Param 0 : t0 - EnvMap
	rootSigBuilder.AddSRVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_ALL);
	// Param 1 : u0 - uav - PrefilterMap
    rootSigBuilder.AddUAVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_ALL);
	// Param 2 : b0 - roughness
	rootSigBuilder.Add32BitConstants(0, 1, 0, D3D12_SHADER_VISIBILITY_ALL);

    D3D12_STATIC_SAMPLER_DESC LinearSampler = {};
    LinearSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    LinearSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    LinearSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    LinearSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

    LinearSampler.MipLODBias = 0;
    LinearSampler.MaxAnisotropy = 16;
    LinearSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    LinearSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;

    LinearSampler.MinLOD = 0.0f;
    LinearSampler.MaxLOD = D3D12_FLOAT32_MAX;
    LinearSampler.ShaderRegister = 0; // s0
    LinearSampler.RegisterSpace = 0;
    LinearSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootSigBuilder.AddStaticSampler(LinearSampler);

	ComputePrefilterRootSignature = rootSigBuilder.Build(Device::GetInstance().GetD3DDevice());

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = ComputePrefilterRootSignature.Get();
    auto CSShader = DXShaderManager::GetInstance().CreateOrFindShader(L"PrefilterMapCompute", L"PrefilterMapCompute.hlsl", "CSMain", "cs_5_0");
    psoDesc.CS = { reinterpret_cast<BYTE*>(CSShader->GetBytecode()->GetBufferPointer()), CSShader->GetBytecode()->GetBufferSize() };
    Device::GetInstance().GetD3DDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&ComputePrefilterPipelineState));


}

void DXRender::ComputePrefilterMap()
{
    CommandList->SetPipelineState(ComputePrefilterPipelineState.Get());
    CommandList->SetComputeRootSignature(ComputePrefilterRootSignature.Get());
    ID3D12DescriptorHeap* ppHeaps[] = {
        DescriptorAllocatorManager::GetInstance().GetCBV_SRV_UAV_Heap()
    };
    CommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    m_EnvCubeMap->BindSRV_Compute(CommandList.Get(), 0);
    for (UINT mip = 0; mip < 5; ++mip)
    {
        float roughness = static_cast<float>(mip) / static_cast<float>(5);
        CommandList->SetComputeRootDescriptorTable(1, m_PrefilterMap->GetUAV_G_ForMip(mip));
        CommandList->SetComputeRoot32BitConstant(2, *reinterpret_cast<UINT*>(&roughness), 0);
        UINT32 mipSize = m_PrefilterMap->GetWidth() >> mip;
        UINT numGroups = max(1, mipSize / 32);
        CommandList->Dispatch(numGroups, numGroups, 6);
	}
}

void DXRender::InitBRDFLUT()
{
    m_BrdfLUTTexture = std::make_shared<Texture>("BRDFLUT");

	TextureDesc brdfDesc;
	brdfDesc.Width = 512;
	brdfDesc.Height = 512;
    brdfDesc.MipLevels = 1;      
	brdfDesc.IsCubeMap = false;
    brdfDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
    brdfDesc.ViewFlags = TextureViewFlags::SRV | TextureViewFlags::UAV;

	m_BrdfLUTTexture->Create(CommandList.Get(), brdfDesc);

    ComputeBRDFLUTRootSignature = ComputePrefilterRootSignature;

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = ComputeBRDFLUTRootSignature.Get();
    auto CSShader = DXShaderManager::GetInstance().CreateOrFindShader(L"BRDFLUTCompute", L"BRDFLUTCompute.hlsl", "CSMain", "cs_5_0");
    psoDesc.CS = { reinterpret_cast<BYTE*>(CSShader->GetBytecode()->GetBufferPointer()), CSShader->GetBytecode()->GetBufferSize() };
    Device::GetInstance().GetD3DDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&ComputeBRDFLUTPipelineState));



}

void DXRender::ComputeBRDFLUT()
{
    CommandList->SetPipelineState(ComputeBRDFLUTPipelineState.Get());
    CommandList->SetComputeRootSignature(ComputeBRDFLUTRootSignature.Get());
    ID3D12DescriptorHeap* ppHeaps[] = {
        DescriptorAllocatorManager::GetInstance().GetCBV_SRV_UAV_Heap()
    };
    CommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	m_BrdfLUTTexture->BindUAV_Compute(CommandList.Get(), 1);
    // 512 / 32 = 16
    CommandList->Dispatch(16, 16, 1);

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
	TempPsoDesc = TestPsoDesc;

	Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&TestPsoDesc, IID_PPV_ARGS(&PipelineState));

	auto TestMaterial = std::make_shared<Material>("TestMaterial");
	MaterialManager::GetInstance().AddMaterial(TestMaterial);

    auto MatGold = std::make_shared<Material>("Mat_Gold");
    MatGold->SetConstantData({
        {1.0f, 0.76f, 0.33f, 1.0f},
        0.2f, 1.0f, 1.0f, 0.0f
        });
    MaterialManager::GetInstance().AddMaterial(MatGold);

    auto MatRedPlastic = std::make_shared<Material>("Mat_Red");
    MatRedPlastic->SetConstantData({
        {1.0f, 0.1f, 0.1f, 1.0f},
        0.5f, 0.0f, 1.0f, 0.0f
        });
    MaterialManager::GetInstance().AddMaterial(MatRedPlastic);

    auto MatWhitePlastic = std::make_shared<Material>("Mat_White");
    MatWhitePlastic->SetConstantData({
        {0.8f, 0.8f, 0.8f, 1.0f},
        0.5f, 0.0f, 1.0f, 0.0f
        });
    MaterialManager::GetInstance().AddMaterial(MatWhitePlastic);


}

void DXRender::CreateFence()
{
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)));
}

void DXRender::PreDraw()
{

}

void DXRender::Draw()
{
    //ThrowIfFailed(CommandAllocator->Reset());
    //ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));

	auto& fr = FrameResources[CurrentFrameResourceIndex];
    if(Fence->GetCompletedValue() < fr.FenceValue)
    {
		HANDLE event = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
        Fence->SetEventOnCompletion(fr.FenceValue, event);
        WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}
    fr.CmdAllocator->Reset();
    ThrowIfFailed(CommandList->Reset(fr.CmdAllocator.Get(), nullptr));
    // ComputeIrradianceMap();


	Material& TestMaterial = MaterialManager::GetInstance().GetOrCreateMaterial("TestMaterial");
    ZPrePass.Execute(CommandList.Get());
	ShadowPass.Execute(CommandList.Get());
    ShadowMaskPass.Execute(CommandList.Get());
    MainPass.Execute(CommandList.Get());
	SkyPass.Execute(CommandList.Get());

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





    // ExecuteCommandAndWaitForComplete();

    ThrowIfFailed(CommandList->Close());
    ID3D12CommandList* cmdLists[] = { CommandList.Get() };
    CommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    ThrowIfFailed(SwapChain3->Present(1, 0));

    fr.FenceValue = ++FenceValue;
    ThrowIfFailed(CommandQueue->Signal(Fence.Get(), fr.FenceValue));


    
    CurrentFrameIdx = SwapChain3->GetCurrentBackBufferIndex();

    CurrentFrameResourceIndex = (CurrentFrameResourceIndex + 1) % FrameResourceNum;
}

DXRender::~DXRender()
{
    if (PtrMesh)
    {
        delete PtrMesh;
    }
    if(SkyboxMesh)
    {
        delete SkyboxMesh;
	}
    for (auto* elem : MeshList)
    {
		if (elem)
		delete elem;
    }
    MaterialManager& Manager = MaterialManager::GetInstance();
 //   for (auto& pair : Manager.Materials)
 //   {
 //       
	//}
}

void DXRender::InitShadowMap()
{

    m_ShadowMap = std::make_shared<Texture>("ShadowMap");
    // 创建 ShadowMap (DSV + SRV)
    m_ShadowMap->Create(
        CommandList.Get(),
        TextureDesc::CreateDepth(ShadowMapSize, ShadowMapSize, true) //SRV (给 MainPass 采样)
    );

    // 初始化视口 (Shadow Pass 专用)
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

		auto& CurrFrameResource = FrameResources[CurrentFrameResourceIndex];

        // Shadow Pass(生成阴影图)
        if (ShadowPSO.Get())
        {
            CommandList->SetPipelineState(ShadowPSO.Get());
        }
        ID3D12DescriptorHeap* ShadowDescriptorHeaps[] = { 
            DescriptorAllocatorManager::GetInstance().GetCBV_SRV_UAV_Heap()
        };
        CommandList->SetDescriptorHeaps(_countof(ShadowDescriptorHeaps), ShadowDescriptorHeaps);
        
		//
		m_ShadowMap->TransitionTo(CommandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_ShadowMap->ClearDepth(CommandList, 1.0f, 0);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvhandle = m_ShadowMap->GetDSV();

        CommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvhandle);

        CommandList->RSSetViewports(1, &m_ShadowViewport);
        CommandList->RSSetScissorRects(1, &m_ShadowScissorRect);

        // Shadow Pass 绘制场景到深度图
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
        // 替换为CBV
        //CommandList->SetGraphicsRootDescriptorTable(1, LightCbvGpuHandle);
		CommandList->SetGraphicsRootConstantBufferView(1, CurrFrameResource.LightConstantBuffer->GetGPUVirtualAddress());


        // XMStoreFloat4x4(&shadowConstants.WorldViewProj, XMMatrixTranspose(MVP));
        XMStoreFloat4x4(&LightConstantInstance.LightViewProj, XMMatrixTranspose(lightViewProj));

        const UINT ObjConstantBufferSize = (sizeof(ObjectConstants) + 255) & ~255;

        for (int i = 0; i < MeshList.size(); ++i)
        {

            auto MeshElement = MeshList[i];
            // 计算 MVP = World * LightView * LightProj
            // 更新常量缓冲区
            auto MVPMatrix = MeshElement->CalMVPMatrix(MainCamera.CalViewProjMatrix());
            auto M_Matrix = MeshElement->GetWorldMatrix();
            ObjectConstants objConstants;
            DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
            DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(M_Matrix));

            memcpy(CurrFrameResource.ObjectConstantBufferMappedData + i * ObjConstantBufferSize, &objConstants, sizeof(objConstants));
			CommandList->SetGraphicsRootConstantBufferView(0, CurrFrameResource.ObjectConstantBuffer->GetGPUVirtualAddress() + i * ObjConstantBufferSize);


            // 绘制
            CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            auto VBView = MeshElement->GetVertexBufferView();
            CommandList->IASetVertexBuffers(0, 1, &VBView);
            auto IBView = MeshElement->GetIndexBufferView();
            CommandList->IASetIndexBuffer(&IBView);
            CommandList->DrawIndexedInstanced(MeshElement->GetIndexCount(), 1, 0, 0, 0);
        }

        // Shadow pass 完成后，将 shadow map 从 depth write 转为 shader resource
		m_ShadowMap->TransitionTo(CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        // 立即执行 shadow pass，避免 CB 被主渲染 pass 覆盖
        //ExecuteCommandAndWaitForComplete();

        // 重置 command list 准备主渲染 pass
        //ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));
	};

    MainPass.Name = "Main Pass";
	MainPass.PSO = PipelineState; // 之前创建的主渲染 PSO
    MainPass.Execute = [this](ID3D12GraphicsCommandList* CommandList)
    {


        auto& CurrFrameResource = FrameResources[CurrentFrameResourceIndex];

        CD3DX12_RESOURCE_BARRIER Barrier_P2RT = CD3DX12_RESOURCE_BARRIER::Transition(
            RenderTargets[CurrentFrameIdx].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        CommandList->ResourceBarrier(1, &Barrier_P2RT);

        D3D12_CPU_DESCRIPTOR_HANDLE CPU_RTV_Handle = RtvHeap->GetCPUDescriptorHandleForHeapStart();
        CPU_RTV_Handle.ptr += CurrentFrameIdx * RtvDescriptorSize;

        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f }; // 深蓝色背景
        CommandList->ClearRenderTargetView(CPU_RTV_Handle, clearColor, 0, nullptr);
		auto dsvhandle = m_SceneDepth->GetDSV();
        CommandList->OMSetRenderTargets(1, &CPU_RTV_Handle, FALSE, &dsvhandle);


        CommandList->RSSetViewports(1, &ScreenViewport);
        CommandList->RSSetScissorRects(1, &ScissorRect);

        CommandList->SetGraphicsRootSignature(RootSignature.Get());

        // 绑定描述符堆
        ID3D12DescriptorHeap* descriptorHeaps[] = { 
            DescriptorAllocatorManager::GetInstance().GetCBV_SRV_UAV_Heap()
        };
        CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

        // 设置主渲染 PSO
        CommandList->SetPipelineState(PipelineState.Get());

        //Light Constants
        auto MainCameraPos = MainCamera.GetPosition();
        LightConstantInstance.CameraPosition = { MainCameraPos.x, MainCameraPos.y,MainCameraPos.z };

        // 将数据拷贝到 Map 好的内存中
        if (CurrFrameResource.LightConstantBufferMappedData)
        {
            memcpy(CurrFrameResource.LightConstantBufferMappedData, &LightConstantInstance, sizeof(LightConstants));
        }
        // CommandList->SetGraphicsRootDescriptorTable(1, LightCbvGpuHandle);
        CommandList->SetGraphicsRootConstantBufferView(1, CurrFrameResource.LightConstantBuffer->GetGPUVirtualAddress());


		//// shadow map SRV
  //      CommandList->SetGraphicsRootDescriptorTable(3, ShadowSRVHandle.GpuHandle);
  //      // shadow mask SRV
		//CommandList->SetGraphicsRootDescriptorTable(4, ShadowMaskSRVHandle.GpuHandle);
		m_ShadowMap->BindSRV_Graphics(CommandList, 3);
		m_ShadowMask->BindSRV_Graphics(CommandList, 4);
		m_IrradianceMap->BindSRV_Graphics(CommandList, 5);
		m_PrefilterMap->BindSRV_Graphics(CommandList, 6);
		m_BrdfLUTTexture->BindSRV_Graphics(CommandList, 7);
        const UINT ObjConstantBufferSize = (sizeof(ObjectConstants) + 255) & ~255;
        const UINT MatConstantBufferSize = (sizeof(MaterialConstants) + 255) & ~255;
        for (int i = 0; i < MeshList.size(); ++i)
        {

            auto MeshElement = MeshList[i];
            {
                // 更新常量缓冲区
                auto MVPMatrix = MeshElement->CalMVPMatrix(MainCamera.CalViewProjMatrix());
                auto M_Matrix = MeshElement->GetWorldMatrix();
                ObjectConstants objConstants;
                DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
                DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(M_Matrix));
                // MeshElement->UpdateObjectConstantBuffer(objConstants);
                memcpy(CurrFrameResource.ObjectConstantBufferMappedData + i * ObjConstantBufferSize, &objConstants, sizeof(objConstants));
                // 绑定 CBV
                // CommandList->SetGraphicsRootDescriptorTable(0, MeshElement->GetCbvGpuHandle());
                CommandList->SetGraphicsRootConstantBufferView(0, CurrFrameResource.ObjectConstantBuffer->GetGPUVirtualAddress() + i * ObjConstantBufferSize);

                auto MaterialName = MeshElement->GetMaterialName();
                Material* MaterialPtr = MaterialManager::GetInstance().GetMaterialByName(MaterialName);

                MaterialConstants matConstants;
                if (MaterialPtr)
                {
                    //MaterialPtr->Bind(CommandList);
                    //std::cout << "Binding Material: " << MaterialName << std::endl;
					memcpy(CurrFrameResource.MaterialConstantBufferMappedData + i * MatConstantBufferSize, &MaterialPtr->GetConstantData(), sizeof(matConstants));
					CommandList->SetGraphicsRootConstantBufferView(2, CurrFrameResource.MaterialConstantBuffer->GetGPUVirtualAddress() + i * MatConstantBufferSize);
                    if (MaterialPtr->HasAlbedoTexture())
                    {
						MaterialPtr->GetAlbedoTexture()->BindSRV_Graphics(CommandList, 8);
                    }
                    else
                    {
                        m_DefaultWhiteTexture->BindSRV_Graphics(CommandList, 8);
                    }
                    if (MaterialPtr->HasNormalTexture())
                    {
                        MaterialPtr->GetNormalTexture()->BindSRV_Graphics(CommandList, 9);
                    }
                    if (MaterialPtr->HasMetallicTexture())
                    {
						MaterialPtr->GetMetallicTexture()->BindSRV_Graphics(CommandList, 10);
                    }

                }
                else
                {
                    m_DefaultWhiteTexture->BindSRV_Graphics(CommandList, 8);
                }
            }

           
            CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            auto VertexBufferView = MeshElement->GetVertexBufferView();
            CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
            auto IndexBufferView = MeshElement->GetIndexBufferView();
            CommandList->IASetIndexBuffer(&IndexBufferView);

            CommandList->DrawIndexedInstanced(MeshElement->GetIndexCount(), 1, 0, 0, 0);
        }
        // MainPass 不执行，等 ImGui 渲染完后统一执行
	};
    
    SkyPass.Execute = [this](ID3D12GraphicsCommandList* CommandList)
        {

            auto& CurrFrameResource = FrameResources[CurrentFrameResourceIndex];

            D3D12_CPU_DESCRIPTOR_HANDLE CPU_RTV_Handle = RtvHeap->GetCPUDescriptorHandleForHeapStart();

            CPU_RTV_Handle.ptr += CurrentFrameIdx * RtvDescriptorSize;

			D3D12_CPU_DESCRIPTOR_HANDLE CPU_DSV_Handle = m_SceneDepth->GetDSV();

            ID3D12DescriptorHeap* descriptorHeaps[] = {
                DescriptorAllocatorManager::GetInstance().GetCBV_SRV_UAV_Heap()
            };
            CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
            CommandList->OMSetRenderTargets(1, &CPU_RTV_Handle, FALSE, &CPU_DSV_Handle);

            CommandList->RSSetViewports(1, &ScreenViewport);
            CommandList->RSSetScissorRects(1, &ScissorRect);

            CommandList->SetGraphicsRootSignature(RootSignature.Get());
            CommandList->SetPipelineState(SkyPass.PSO.Get());
            //Light Constants
            auto MainCameraPos = MainCamera.GetPosition();
            SkyboxMesh->SetPosition(MainCameraPos.x, MainCameraPos.y, MainCameraPos.z);
            LightConstantInstance.CameraPosition = { MainCameraPos.x, MainCameraPos.y,MainCameraPos.z };

            if (CurrFrameResource.LightConstantBufferMappedData)
            {
                memcpy(CurrFrameResource.LightConstantBufferMappedData, &LightConstantInstance, sizeof(LightConstants));
            }
            // CommandList->SetGraphicsRootDescriptorTable(1, LightCbvGpuHandle);
            CommandList->SetGraphicsRootConstantBufferView(1, CurrFrameResource.LightConstantBuffer->GetGPUVirtualAddress());

            CommandList->SetGraphicsRootDescriptorTable(3, m_EnvCubeMap->GetSRV_G());

            auto MVPMatrix = SkyboxMesh->CalMVPMatrix(MainCamera.CalViewProjMatrix());
            auto M_Matrix = SkyboxMesh->GetWorldMatrix();
            ObjectConstants objConstants;
            DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
            DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(M_Matrix));

            // Skybox 用大 buffer 最后一个槽位
            // SkyboxMesh->UpdateObjectConstantBuffer(objConstants); // OLD
            const int SkyboxCBIndex = (int)MeshList.size();
            const UINT ObjCBSize = (sizeof(ObjectConstants) + 255) & ~255;

            memcpy(CurrFrameResource.ObjectConstantBufferMappedData + SkyboxCBIndex * ObjCBSize,
                &objConstants, sizeof(ObjectConstants));

            CommandList->SetGraphicsRootConstantBufferView(0,
                CurrFrameResource.ObjectConstantBuffer->GetGPUVirtualAddress() + SkyboxCBIndex * ObjCBSize);

			auto VBView = SkyboxMesh->GetVertexBufferView();
			auto IBView = SkyboxMesh->GetIndexBufferView();
            // CommandList->SetGraphicsRootDescriptorTable(0, SkyboxMesh->GetCbvGpuHandle());
            CommandList->IASetIndexBuffer(&IBView);
			CommandList->IASetVertexBuffers(0, 1, &VBView);
            CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			CommandList->DrawIndexedInstanced(SkyboxMesh->GetIndexCount(), 1, 0, 0, 0);


        };
    ShadowMaskPass.Name = "ShadowMaskPass";
    ShadowMaskPass.Execute = [&](ID3D12GraphicsCommandList* CommandList)
        {

            auto& CurrFrameResource = FrameResources[CurrentFrameResourceIndex];

            m_ShadowMask->Clear(CommandList);
            // m_SceneDepth
			m_ShadowMask->SetAsRenderTarget(CommandList, m_SceneDepth.get());

            CommandList->RSSetViewports(1, &ScreenViewport);
            CommandList->RSSetScissorRects(1, &ScissorRect);

            // 绑定描述符堆
            ID3D12DescriptorHeap* descriptorHeaps[] = { 
                DescriptorAllocatorManager::GetInstance().GetCBV_SRV_UAV_Heap() 
            };
            CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

            CommandList->SetGraphicsRootSignature(RootSignature.Get());
            CommandList->SetGraphicsRootDescriptorTable(3, m_ShadowMap->GetSRV_G()); // 绑定 shadow map SRV
			CommandList->SetPipelineState(ShadowMaskPass.PSO.Get());

            //Light Constants
            auto MainCameraPos = MainCamera.GetPosition();
            LightConstantInstance.CameraPosition = { MainCameraPos.x, MainCameraPos.y,MainCameraPos.z };

            if (CurrFrameResource.LightConstantBufferMappedData)
            {
                memcpy(CurrFrameResource.LightConstantBufferMappedData, &LightConstantInstance, sizeof(LightConstants));
            }
            // CommandList->SetGraphicsRootDescriptorTable(1, LightCbvGpuHandle);
            CommandList->SetGraphicsRootConstantBufferView(1, CurrFrameResource.LightConstantBuffer->GetGPUVirtualAddress());

			const UINT ObjConstantBufferSize = (sizeof(ObjectConstants) + 255) & ~255;
            for (int i = 0; i < MeshList.size(); ++i)
            {

                auto MeshElement = MeshList[i];
                {
                    // 更新常量缓冲区
                    auto MVPMatrix = MeshElement->CalMVPMatrix(MainCamera.CalViewProjMatrix());
                    auto M_Matrix = MeshElement->GetWorldMatrix();
                    ObjectConstants objConstants;
                    DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
                    DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(M_Matrix));
                    // MeshElement->UpdateObjectConstantBuffer(objConstants);

                    memcpy(CurrFrameResource.ObjectConstantBufferMappedData + i * ObjConstantBufferSize, &objConstants, sizeof(objConstants));
                }

                // 绑定 CBV
                // CommandList->SetGraphicsRootDescriptorTable(0, ConstantBufferViewHeap->GetGPUDescriptorHandleForHeapStart());
                // CommandList->SetGraphicsRootDescriptorTable(0, MeshElement->GetCbvGpuHandle());
				CommandList->SetGraphicsRootConstantBufferView(0, CurrFrameResource.ObjectConstantBuffer->GetGPUVirtualAddress() + i * ObjConstantBufferSize);

                CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                auto VertexBufferView = MeshElement->GetVertexBufferView();
                CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
                auto IndexBufferView = MeshElement->GetIndexBufferView();
                CommandList->IASetIndexBuffer(&IndexBufferView);

                CommandList->DrawIndexedInstanced(MeshElement->GetIndexCount(), 1, 0, 0, 0);
            }
			m_ShadowMask->TransitionTo(CommandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        };

    ZPrePass.Name = "ZPrepass";

    ZPrePass.Execute = [this](ID3D12GraphicsCommandList* CommandList)
        {

            auto& CurrFrameResource = FrameResources[CurrentFrameResourceIndex];
            // ZPrepass implementation (similar to ShadowPass but for main camera)
            // You can fill this in as needed
                        // 绑定描述符堆
            ID3D12DescriptorHeap* descriptorHeaps[] = {
                DescriptorAllocatorManager::GetInstance().GetCBV_SRV_UAV_Heap()
            };
            CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle = m_SceneDepth->GetDSV();
			CommandList->ClearDepthStencilView(DsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
			CommandList->OMSetRenderTargets(0, nullptr, FALSE, &DsvHandle);

            CommandList->RSSetViewports(1, &ScreenViewport);
            CommandList->RSSetScissorRects(1, &ScissorRect);

			CommandList->SetGraphicsRootSignature(RootSignature.Get());
			CommandList->SetPipelineState(ZPrePass.PSO.Get());

			const UINT ObjConstantBufferSize = (sizeof(ObjectConstants) + 255) & ~255;

            for (int i = 0; i < MeshList.size(); ++i)
            {

                auto MeshElement = MeshList[i];

                // 更新常量缓冲区
                auto MVPMatrix = MeshElement->CalMVPMatrix(MainCamera.CalViewProjMatrix());
                auto M_Matrix = MeshElement->GetWorldMatrix();
                ObjectConstants objConstants;
                DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
                DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(M_Matrix));
                // MeshElement->UpdateObjectConstantBuffer(objConstants);
                memcpy(CurrFrameResource.ObjectConstantBufferMappedData + i * ObjConstantBufferSize, &objConstants, sizeof(objConstants));

                // cbv
                // CommandList->SetGraphicsRootDescriptorTable(0, MeshElement->GetCbvGpuHandle());
                CommandList->SetGraphicsRootConstantBufferView(0, CurrFrameResource.ObjectConstantBuffer->GetGPUVirtualAddress() + i * ObjConstantBufferSize);

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
    m_ShadowMask = std::make_shared<Texture>("ShadowMask");
    // 创建 ShadowMask (RTV + SRV, LDR格式)
    m_ShadowMask->Create(
        CommandList.Get(),
        TextureDesc::Create2D(Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM, TextureViewFlags::RTV | TextureViewFlags::SRV)
    );
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

void DXRender::InitSkyPassPSO()
{
    auto SkyVS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"SkyVS", L"Skybox.hlsl", "VSMain", "vs_5_0"
    )->GetBytecode();
    auto SkyPS = DXShaderManager::GetInstance().CreateOrFindShader(
        L"SkyPS", L"Skybox.hlsl", "PSMain", "ps_5_0"
    )->GetBytecode();
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { StandardVertexInputLayout, _countof(StandardVertexInputLayout) };
    psoDesc.pRootSignature = RootSignature.Get(); // 复用现在的 RootSig
    psoDesc.VS = { reinterpret_cast<BYTE*>(SkyVS->GetBufferPointer()), SkyVS->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(SkyPS->GetBufferPointer()), SkyPS->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = TRUE;  // 临时禁用深度测试进行调试
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&SkyPass.PSO)));
}



DXRender::DXRender()
{
    MainCamera.Init((float)Width, (float)Height);
}

// not use anymore
DescriptorAllocation DXRender::AllocateDescriptorHandle(unsigned int DescriptorSize)
{
    if(CurrentSrvHeapIndex >= MAX_HEAP_SIZE)
    {
        throw std::runtime_error("Exceeded maximum descriptor heap size!");
	}
    // 计算 CPU 句柄 (用于创建资源)
    //CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(
    //    ConstantBufferViewHeap->GetCPUDescriptorHandleForHeapStart(),
    //    CurrentSrvHeapIndex,
    //    DescriptorSize);

    //// 计算 GPU 句柄 (用于绑定 Shader)
    //CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(
    //    ConstantBufferViewHeap->GetGPUDescriptorHandleForHeapStart(),
    //    CurrentSrvHeapIndex,
    //    DescriptorSize);

	unsigned int RetIdx = CurrentSrvHeapIndex;

    CurrentSrvHeapIndex++; // 指针后移

    //return { cpuHandle, gpuHandle, RetIdx };
    return { 0, 0, RetIdx };

    //return std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>();
}

// DepthStencilBuffer
void DXRender::InitDepthStencilBuffer()
{

    m_SceneDepth = std::make_shared<Texture>("SceneDepth");
    m_SceneDepth->Create(
        CommandList.Get(),
        TextureDesc::CreateDepth(Width, Height, true)
	);

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

void FrameResource::Init(ID3D12Device* device, UINT maxObjectCount)
{
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CmdAllocator));

    // Light Constant Buffer
    const UINT LightConstantBufferSize = (sizeof(LightConstants) + 255) & ~255;

    // 创建上传堆的常量缓冲资源
    CD3DX12_HEAP_PROPERTIES LightHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(LightConstantBufferSize);
    ThrowIfFailed(device->CreateCommittedResource(&LightHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&LightConstantBuffer)));
    // 2) 映射得到 CPU 可写指针
    CD3DX12_RANGE ReadRange(0, 0);
    LightConstantBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&LightConstantBufferMappedData));


    // Obj Constant Buffer
    const UINT ObjConstantBufferSize = (sizeof(ObjectConstants) + 255) & ~255;
    const UINT MaxObjectNum = 128;
    CD3DX12_HEAP_PROPERTIES ObjHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC ObjBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ObjConstantBufferSize * MaxObjectNum);
    ThrowIfFailed(device->CreateCommittedResource(&ObjHeapProps, D3D12_HEAP_FLAG_NONE, &ObjBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&ObjectConstantBuffer)));
    ObjectConstantBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&ObjectConstantBufferMappedData));


	// Material Constant Buffer
	const UINT MaterialConstantBufferSize = (sizeof(MaterialConstants) + 255) & ~255;
    const UINT MaxMaterialNum = 128;

    CD3DX12_HEAP_PROPERTIES MatHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC MatBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(MaterialConstantBufferSize * MaxMaterialNum);
    ThrowIfFailed(device->CreateCommittedResource(&MatHeapProps, D3D12_HEAP_FLAG_NONE, &MatBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&MaterialConstantBuffer)));
    MaterialConstantBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&MaterialConstantBufferMappedData));




}
