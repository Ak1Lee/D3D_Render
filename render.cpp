#include "render.h"


#include <iostream>
#include <future>
#include <array>
#include <chrono>
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
#include "EditorUI.h"

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
    // 鍒濆鍖栫獥鍙ｇ被鍜屽嚱鏁?
    WNDCLASS wc = {};
    //wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MyWindowClass";
    wc.lpfnWndProc = GlobalWndProc;
    RegisterClass(&wc);

    // 鍒涘缓绐楀彛
    HWND hwnd = CreateWindow(
        L"MyWindowClass", L"My DX12 App",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        Width, Height, nullptr, nullptr, hInstance, this);

    ShowWindow(hwnd, nCmdShow);

    InitDX(hwnd);

    ThrowIfFailed(CommandAllocator->Reset());
    ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));
    InitTextures();
    InitPasses_new();
    // InitEnvCubeMap 闇€瑕?CommandList 澶勪簬鎵撳紑鐘舵€?
    InitEnvCubeMapAndIrradianceMap();
    InitPrefilterRootSignature();
    InitBRDFLUT();


	//HDRTex.LoadHDRFromFile(CommandList.Get(), ".\\resources\\puresky_2k.hdr");
	m_HDRSkyTexture = std::make_shared<Texture>("HDRSky");
	m_HDRSkyTexture->LoadFromFile(Device::GetInstance().GetD3DDevice(),CommandList.Get(), ".\\resources\\venice_sunset_1k.hdr",false,true);

    ExecuteCommandAndWaitForComplete();

    

    // imgui - 鍦?InitDX 涔嬪悗鍒濆鍖?
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1; // ImGui 鍙渶瑕佷竴涓綅缃瓨瀛椾綋璐村浘
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // 蹇呴』鏄?Shader 鍙鐨?
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&ImguiSrvHeap)));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.Fonts->AddFontDefault();
    //io.Fonts->Build();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);

    //HRESULT hr = ImGui_ImplDX12_Init(Device::GetInstance().GetD3DDevice(), FrameBufferCount,
    //    DXGI_FORMAT_R8G8B8A8_UNORM, ImguiSrvHeap.Get(),
    //    ImguiSrvHeap->GetCPUDescriptorHandleForHeapStart(),
    //    ImguiSrvHeap->GetGPUDescriptorHandleForHeapStart());
    //if (FAILED(hr)) {
    //    MessageBox(NULL, L"ImGui_ImplDX12_Init Failed!", L"Error", MB_OK);
    //    return;
    //}
    // 鏇挎崲鏃х殑 ImGui_ImplDX12_Init 璋冪敤锛?
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = Device::GetInstance().GetD3DDevice();
    init_info.CommandQueue = CommandQueue.Get();  // 鍏抽敭锛佷紶鍏ヤ綘鐨?CommandQueue
    init_info.NumFramesInFlight = FrameBufferCount;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    // 浣跨敤浣犵殑鎻忚堪绗﹀爢
    init_info.SrvDescriptorHeap = ImguiSrvHeap.Get();
    init_info.LegacySingleSrvCpuDescriptor = ImguiSrvHeap->GetCPUDescriptorHandleForHeapStart();
    init_info.LegacySingleSrvGpuDescriptor = ImguiSrvHeap->GetGPUDescriptorHandleForHeapStart();
    ImGui_ImplDX12_Init(&init_info);

    m_UI = new EditorUI(this);

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
	InitDeferredLightRootSignature();
    InitComputeRootSignature();
    InitIrradianceMapCompute();

    CompileShader();



    CreateFence();
    ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), nullptr));

    CreateConstantBufferView();

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
		// DescriptorHandle AllocInfo = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
        // BoxPtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), AllocInfo);
		BoxPtr->SetMaterialByName("Mat_Red");
        MeshList.push_back(BoxPtr);
    }
    // 榛樿 1x1 鐧借壊璐村浘
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
        // 璁＄畻閲戝睘搴?(0.0 -> 1.0)
        float metallic = (float)y / (float)(rows - 1);

        for (int x = 0; x < cols; ++x)
        {
            // 璁＄畻绮楃硻搴?(0.05 -> 1.0)
            // Clamp 鍒?0.05 鏄负浜嗛槻姝㈤櫎0閿欒瀵艰嚧浜偣闂儊
            float roughness = (std::max)((float)x / (float)(cols - 1), 0.05f);

            //鍔ㄦ€佸垱寤轰竴涓潗璐?
            // 缁欏畠璧蜂釜鍞竴鐨勫悕瀛楋紝姣斿 "Mat_Test_0_0", "Mat_Test_0_1"
            std::string matName = "Mat_Test_" + std::to_string(x) + "_" + std::to_string(y);

            auto tempMat = std::make_shared<Material>(matName);
            tempMat->SetConstantData({
                {1.0f, 0.0f, 0.0f, 1.0f},
                roughness, metallic, 1.0f, 0.0f
                });
            MaterialManager::GetInstance().AddMaterial(tempMat);


            // 4. 鍒涘缓鐞冧綋
            auto SpherePtr = new Sphere();

            // 灞呬腑鎺掑垪浣嶇疆
            float posX = (x - (cols / 2)) * spacing;
            float posY = (y - (rows / 2)) * spacing + 10;
            SpherePtr->SetPosition(posX, posY, 0.0f);

            SpherePtr->InitVertexBufferAndIndexBuffer(Device::GetInstance().GetD3DDevice(), CommandList.Get());

            // DescriptorHandle AllocInfo = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
            // SpherePtr->InitObjectConstantBuffer(Device::GetInstance().GetD3DDevice(), AllocInfo);

            // 5. 缁戝畾鍒氭墠鍒涘缓鐨勬潗璐?
            // 濡傛灉浣犵殑 Sphere 鏀寔鐩存帴浼犳寚閽堬細
            // SpherePtr->SetMaterial(tempMat);

            // 鎴栬€呭鏋滃繀椤讳紶鍚嶅瓧锛?
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

    // 鎵ц鍒濆鍖栧懡浠?
    ExecuteCommandAndWaitForComplete();
}

void DXRender::ExecuteCommandAndWaitForComplete()
{
    ThrowIfFailed(CommandList->Close());
    ID3D12CommandList* cmdLists[] = { CommandList.Get() };
    CommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // 绛夊緟GPU瀹屾垚鍒濆鍖?
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
    //鍒涘缓鍛戒护闃熷垪
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
    // RenderTargetView Heap 鍜?RenderTargetView 鎻忚堪绗﹀ぇ灏?
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

    // 鍒涘缓甯搁噺缂撳啿鍖?- 澶у皬蹇呴』鏄?56瀛楄妭瀵归綈 杞Щ鍒?MeshBase
 //   const UINT constantBufferSize = (sizeof(ObjectConstants) + 255) & ~255;

    //CD3DX12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    //CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
 //   ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateCommittedResource(&HeapProps,D3D12_HEAP_FLAG_NONE,&BufferDesc,
 //       D3D12_RESOURCE_STATE_GENERIC_READ,
 //       nullptr,
 //       IID_PPV_ARGS(&ObjectConstantBuffer)));
 //   // 鏄犲皠骞跺垵濮嬪寲
 //   CD3DX12_RANGE readRange(0, 0);
 //   ThrowIfFailed(ObjectConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&ConstantBufferMappedData)));
 //   // ZeroMemory(ConstantBufferMappedData, constantBufferSize);
 //   // 鍒涘缓CBV
 //   D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
 //   cbvDesc.BufferLocation = ObjectConstantBuffer->GetGPUVirtualAddress();
 //   cbvDesc.SizeInBytes = constantBufferSize; // 蹇呴』鏄?56瀛楄妭瀵归綈
    //Device::GetInstance().GetD3DDevice()->CreateConstantBufferView(&cbvDesc, ConstantBufferViewHeap->GetCPUDescriptorHandleForHeapStart());

	// Light Constant Buffer
	const UINT LightConstantBufferSize = (sizeof(LightConstants) + 255) & ~255;

    // 鍒涘缓涓婁紶鍫嗙殑甯搁噺缂撳啿璧勬簮
    CD3DX12_HEAP_PROPERTIES LightHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(LightConstantBufferSize);
    ThrowIfFailed(Device::GetInstance().GetD3DDevice()->CreateCommittedResource(&LightHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
		IID_PPV_ARGS(&LightConstantBuffer)));
    // 2) 鏄犲皠寰楀埌 CPU 鍙啓鎸囬拡
    CD3DX12_RANGE ReadRange(0, 0);
    ThrowIfFailed(LightConstantBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&LightConstantBufferMappedData)));
    auto LightCvbHandle = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
	LightCbvCpuHandle = LightCvbHandle.CpuHandle;
	LightCbvGpuHandle = LightCvbHandle.GpuHandle;
    // 鍒涘缓 CBV 
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
    // 鏍瑰弬鏁?鈥斺€?index 灏辨槸娣诲姞椤哄簭
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
    // --- IBL 璐村浘琛?(Index 5) ---
    // t10, t11, t12
    // Index 5: t10 (Irradiance)
    rootSigBuilder.AddSRVDescriptorTable(10, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // Index 6: t11 (Prefilter)
    rootSigBuilder.AddSRVDescriptorTable(11, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // Index 7: t12 (BRDF LUT)
    rootSigBuilder.AddSRVDescriptorTable(12, 1, D3D12_SHADER_VISIBILITY_PIXEL);

    // pbr tetxure
	// --- PBR 璐村浘琛?(Index 8) abedo ---
    rootSigBuilder.AddSRVDescriptorTable(13, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // --- PBR 璐村浘琛?(Index 9) normal---
    rootSigBuilder.AddSRVDescriptorTable(14, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    // --- PBR 璐村浘琛?(Index 10) metallic---
    rootSigBuilder.AddSRVDescriptorTable(15, 1, D3D12_SHADER_VISIBILITY_PIXEL);


    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; // 瑙嗙晫澶栬涓烘渶杩滄繁搴?
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0; // s0
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootSigBuilder.AddStaticSampler(sampler);

    D3D12_STATIC_SAMPLER_DESC samplerShadow = {};
    samplerShadow.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; // 寮€鍚瘮杈?+ 绾挎€ф护娉?
    samplerShadow.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplerShadow.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplerShadow.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplerShadow.MipLODBias = 0;
    samplerShadow.MaxAnisotropy = 0;
    samplerShadow.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 灏忎簬绛変簬鍒欎负浜?
    samplerShadow.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplerShadow.MinLOD = 0.0f;
    samplerShadow.MaxLOD = D3D12_FLOAT32_MAX;
    samplerShadow.ShaderRegister = 1; // 缁戝畾鍒?s1
    samplerShadow.RegisterSpace = 0;
    samplerShadow.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootSigBuilder.AddStaticSampler(samplerShadow);

    D3D12_STATIC_SAMPLER_DESC samplerLinear = {};
    samplerLinear.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 绾挎€?
    samplerLinear.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerLinear.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerLinear.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerLinear.MipLODBias = 0;
    samplerLinear.MaxAnisotropy = 16;
    samplerLinear.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerLinear.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplerLinear.MinLOD = 0.0f;
    samplerLinear.MaxLOD = D3D12_FLOAT32_MAX;
    samplerLinear.ShaderRegister = 2; // <--- 鍏抽敭锛氱粦瀹氬埌 s2
    samplerLinear.RegisterSpace = 0;
    samplerLinear.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootSigBuilder.AddStaticSampler(samplerLinear);

    //IBL Linear + Clamp
    D3D12_STATIC_SAMPLER_DESC samplerIBL = {};
    samplerIBL.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 蹇呴』鏄嚎鎬?
    samplerIBL.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // 銆愬叧閿€慍lamp
    samplerIBL.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; // 銆愬叧閿€慍lamp
    samplerIBL.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerIBL.MipLODBias = 0;
    samplerIBL.MaxAnisotropy = 16;
    samplerIBL.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerIBL.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplerIBL.MinLOD = 0.0f;
    samplerIBL.MaxLOD = D3D12_FLOAT32_MAX;
    samplerIBL.ShaderRegister = 3; // 缁戝畾鍒?s3
    samplerIBL.RegisterSpace = 0;
    samplerIBL.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootSigBuilder.AddStaticSampler(samplerIBL);

    RootSignature = rootSigBuilder.Build(Device::GetInstance().GetD3DDevice());

}
void DXRender::InitDeferredLightRootSignature()
{
    DXRootSignature rootSigBuilder;

    // Index 0: b0 Light CB (Root CBV)
    rootSigBuilder.AddRootConstantBufferView(0);


    // t0 GBuffer0
	rootSigBuilder.AddSRVDescriptorTable(0, 1, D3D12_SHADER_VISIBILITY_PIXEL);

	// t1 GBuffer1
	rootSigBuilder.AddSRVDescriptorTable(1, 1, D3D12_SHADER_VISIBILITY_PIXEL);

	// t2 GBuffer2
	rootSigBuilder.AddSRVDescriptorTable(2, 1, D3D12_SHADER_VISIBILITY_PIXEL);

    // t3 GBuffer3
    rootSigBuilder.AddSRVDescriptorTable(3, 1, D3D12_SHADER_VISIBILITY_PIXEL);

	// t4 Shadow Mask
    rootSigBuilder.AddSRVDescriptorTable(4, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	// t5 Irradiance Map
    rootSigBuilder.AddSRVDescriptorTable(5, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	// t6 Prefilter Map
    rootSigBuilder.AddSRVDescriptorTable(6, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	// t7 BRDF LUT
    rootSigBuilder.AddSRVDescriptorTable(7, 1, D3D12_SHADER_VISIBILITY_PIXEL);
	// t8 depth
    rootSigBuilder.AddSRVDescriptorTable(8, 1, D3D12_SHADER_VISIBILITY_PIXEL);


    //sampler
    // point sampler
    D3D12_STATIC_SAMPLER_DESC samplerPointClamp = {};
    samplerPointClamp.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplerPointClamp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerPointClamp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerPointClamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerPointClamp.MipLODBias = 0;
    samplerPointClamp.MaxAnisotropy = 0;
    samplerPointClamp.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerPointClamp.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplerPointClamp.MinLOD = 0.0f;
    samplerPointClamp.MaxLOD = D3D12_FLOAT32_MAX;
    samplerPointClamp.ShaderRegister = 0; // 銆愬叧閿€戝繀椤绘槸 s0 鍖归厤 shader
    samplerPointClamp.RegisterSpace = 0;
    samplerPointClamp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    //PCF 
    D3D12_STATIC_SAMPLER_DESC samplerShadow = {};
    samplerShadow.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; 
    samplerShadow.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplerShadow.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplerShadow.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    samplerShadow.MipLODBias = 0;
    samplerShadow.MaxAnisotropy = 0;
    samplerShadow.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; 
    samplerShadow.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplerShadow.MinLOD = 0.0f;
    samplerShadow.MaxLOD = D3D12_FLOAT32_MAX;
    samplerShadow.ShaderRegister = 1; // 銆愬叧閿€戝繀椤绘槸 s1 鍖归厤 shader
    samplerShadow.RegisterSpace = 0;
    samplerShadow.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    //IBL Linear + Clamp
    D3D12_STATIC_SAMPLER_DESC samplerLinear = {};
    samplerLinear.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; 
    samplerLinear.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; 
    samplerLinear.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; 
    samplerLinear.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerLinear.MipLODBias = 0;
    samplerLinear.MaxAnisotropy = 0; // 銆愬叧閿€戝鏋?Filter 涓嶆槸 ANISOTROPIC锛岃繖閲屽繀椤讳负 0锛屽惁鍒?Build 浼氭姤閿欙紒
    samplerLinear.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerLinear.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplerLinear.MinLOD = 0.0f;
    samplerLinear.MaxLOD = D3D12_FLOAT32_MAX;
    samplerLinear.ShaderRegister = 2; // 銆愬叧閿€戝繀椤绘槸 s2 鍖归厤 shader
    samplerLinear.RegisterSpace = 0;
    samplerLinear.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootSigBuilder.AddStaticSampler(samplerPointClamp);
    rootSigBuilder.AddStaticSampler(samplerShadow);
    rootSigBuilder.AddStaticSampler(samplerLinear);
    m_DeferredRootSignature = rootSigBuilder.Build(Device::GetInstance().GetD3DDevice());


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
	// 璁剧疆鎻忚堪绗﹀爢
	//CommandList->SetComputeRootDescriptorTable(0, m_HDRSkyTexture->GetSRV_G());
    m_HDRSkyTexture->BindSRV_Compute(CommandList.Get(), 0);
	// CommandList->SetComputeRootDescriptorTable(1, m_EnvCubeMap->GetUAV_G());
    m_EnvCubeMap->BindUAV_Compute(CommandList.Get(), 1);

	CommandList->Dispatch(1024 / 32, 1024 / 32, 6); // 姣忎釜绾跨▼缁勫鐞?6x16鍍忕礌

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
    cubeMipDesc.Width = 128; // 璧峰澶у皬
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

void DXRender::InitGBuffers()
{
    m_GBuffer0 = std::make_shared<Texture>("GBuffer0");
    m_GBuffer1 = std::make_shared<Texture>("GBuffer1");
    m_GBuffer2 = std::make_shared<Texture>("GBuffer2");

    TextureDesc GBuffer0Desc;
	GBuffer0Desc.Width = Width;
	GBuffer0Desc.Height = Height;
    GBuffer0Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    GBuffer0Desc.ViewFlags = TextureViewFlags::RTV | TextureViewFlags::SRV | TextureViewFlags::UAV;
	m_GBuffer0->Create(CommandList.Get(), GBuffer0Desc);

    TextureDesc GBuffer1Desc;
    GBuffer1Desc.Width = Width;
    GBuffer1Desc.Height = Height;
    GBuffer1Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    GBuffer1Desc.ViewFlags = TextureViewFlags::RTV | TextureViewFlags::SRV | TextureViewFlags::UAV;
    m_GBuffer1->Create(CommandList.Get(), GBuffer1Desc);

    TextureDesc GBuffer2Desc;
    GBuffer2Desc.Width = Width;
    GBuffer2Desc.Height = Height;
    GBuffer2Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    GBuffer2Desc.ViewFlags = TextureViewFlags::RTV | TextureViewFlags::SRV | TextureViewFlags::UAV;
    m_GBuffer2->Create(CommandList.Get(), GBuffer2Desc);


}

void DXRender::CompileShader()
{

	DXShaderManager::GetInstance().CreateOrFindShader(L"TestVS", L"PBRShader.hlsl", "VSMain", "vs_5_0");
    DXShaderManager::GetInstance().CreateOrFindShader(L"TestPS", L"PBRShader.hlsl", "PSMain", "ps_5_0");

}

void DXRender::InitInputLayout()
{
    // 杈撳叆甯冨眬鎻忚堪
    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void DXRender::InitPSO()
{
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
	// 2025.12.04 涓嶅啀浣跨敤TestInputLayout锛屼娇鐢–ommon.h涓殑StandardVertexInputLayout 瀵瑰簲鐨刅ertex缁撴瀯浣擄細StandardVertex
    // 杈撳叆甯冨眬鎻忚堪
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
    auto recordStart = std::chrono::high_resolution_clock::now();

    /* old version*/
    /*

    // Pass 鎵ц鍑芥暟琛紙椤哄簭涓?EPassIndex 瀵瑰簲锛?
    std::function<void(ID3D12GraphicsCommandList*)> passExecs[PASS_COUNT] = {
        ZPrePass.Execute,
        ShadowPass.Execute,
        ShadowMaskPass.Execute,
        MainPass.Execute,
        SkyPass.Execute,
    };
    if (bEnableMultiThreadRecord)
    {
        // 澶氱嚎绋嬪綍鍒讹細姣忎釜 Pass 鍦ㄨ嚜宸辩殑绾跨▼閲?Reset/Execute/Close
        std::future<ID3D12GraphicsCommandList*> futures[PASS_COUNT];
        for (int i = 0; i < PASS_COUNT; ++i)
        {
            futures[i] = std::async(std::launch::async,
                [i, &fr, &passExecs]() -> ID3D12GraphicsCommandList*
                {
                    auto* alloc = fr.PassCmdAllocators[i].Get();
                    auto* cl = fr.PassCmdLists[i].Get();
                    alloc->Reset();
                    cl->Reset(alloc, nullptr);
                    passExecs[i](cl);
                    cl->Close();
                    return cl;
                });
        }
        // 鎸夐『搴忔敹闆嗗苟鎻愪氦
        ID3D12CommandList* submitList[PASS_COUNT];
        for (int i = 0; i < PASS_COUNT; ++i)
        {
            submitList[i] = futures[i].get();
        }
        CommandQueue->ExecuteCommandLists(PASS_COUNT, submitList);
    }
    else
    {
        // 鍗曠嚎绋嬭矾寰勶細鍏ㄩ儴褰曞埌涓?CmdList
        for (int i = 0; i < PASS_COUNT; ++i)
        {
            passExecs[i](CommandList.Get());
        }
    }
    */

    // 褰曞埗鍛戒护
    m_PassManager.ExecuteAllPasses(CommandList.Get());

    auto recordEnd = std::chrono::high_resolution_clock::now();
    m_LastRecordTimeMs = std::chrono::duration<float, std::milli>(recordEnd - recordStart).count();

    // --- Start ImGui Frame ---
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // --- Build UI ---
    if (m_UI)
    {
        m_UI->Draw();
    }

    ImGui::Render();
    
    // ImGui 娓叉煋鍒板綋鍓?RT锛圡T 妯″紡涓嬩富 CL 杩樻病缁?RTV锛岃繖閲屾樉寮忕粦涓€娆★級
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = RtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += CurrentFrameIdx * RtvDescriptorSize;
        CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    }
    // 鍒囨崲鍒?ImGui 鐨?descriptor heap 骞舵覆鏌?
    ID3D12DescriptorHeap* imguiHeaps[] = { ImguiSrvHeap.Get()};
    CommandList->SetDescriptorHeaps(1, imguiHeaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), CommandList.Get());
    
    // --- 涓€甯х粨鏉?---
    // 杞崲褰撳墠杩欎釜rt鐨勭姸鎬佸埌present
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
    if (m_UI)
    {
        delete m_UI;
        m_UI = nullptr;
    }
}

void DXRender::InitPasses_new()
{

    IRenderPass* zPrePass = m_PassManager.AddPass<ZPrePass>();
	zPrePass->SetRootSignature(RootSignature.Get());
	zPrePass->SetPipelineState(m_zPrePassPSO.Get());
    zPrePass->AddDependency(m_SceneDepth.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    zPrePass->Execute = [this](ID3D12GraphicsCommandList* CommandList)
        {
            auto& CurrFrameResource = FrameResources[CurrentFrameResourceIndex];
            // ZPrepass implementation
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

            const UINT ObjConstantBufferSize = (sizeof(ObjectConstants) + 255) & ~255;

            for (int i = 0; i < MeshList.size(); ++i)
            {

                auto MeshElement = MeshList[i];
                if (!MeshElement->IsVisible()) continue;

                // 鏇存柊甯搁噺缂撳啿鍖?
                auto MVPMatrix = MeshElement->CalMVPMatrix(MainCamera.CalViewProjMatrix());
                auto M_Matrix = MeshElement->GetWorldMatrix();
                ObjectConstants objConstants;
                DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
                DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(M_Matrix));
                // MeshElement->UpdateObjectConstantBuffer(objConstants);
                memcpy(CurrFrameResource.ObjectConstantBufferMappedData + i * ObjConstantBufferSize, &objConstants, sizeof(objConstants));

                // cbv
                CommandList->SetGraphicsRootConstantBufferView(0, CurrFrameResource.ObjectConstantBuffer->GetGPUVirtualAddress() + i * ObjConstantBufferSize);

                CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                auto VertexBufferView = MeshElement->GetVertexBufferView();
                CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
                auto IndexBufferView = MeshElement->GetIndexBufferView();
                CommandList->IASetIndexBuffer(&IndexBufferView);

                CommandList->DrawIndexedInstanced(MeshElement->GetIndexCount(), 1, 0, 0, 0);

            }
		};

	IRenderPass* shadowPass = m_PassManager.AddPass<ShadowPass>();
	shadowPass->SetRootSignature(RootSignature.Get());
    shadowPass->AddDependency(m_ShadowMap.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
    shadowPass->Execute = [this](ID3D12GraphicsCommandList* CommandList){
       	auto& CurrFrameResource = FrameResources[CurrentFrameResourceIndex];
        // Shadow Pass(鐢熸垚闃村奖鍥?
        ID3D12DescriptorHeap* ShadowDescriptorHeaps[] = { 
            DescriptorAllocatorManager::GetInstance().GetCBV_SRV_UAV_Heap()
        };
        CommandList->SetDescriptorHeaps(_countof(ShadowDescriptorHeaps), ShadowDescriptorHeaps);
       	m_ShadowMap->ClearDepth(CommandList, 1.0f, 0);
       	D3D12_CPU_DESCRIPTOR_HANDLE dsvhandle = m_ShadowMap->GetDSV();
        CommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvhandle);
        CommandList->RSSetViewports(1, &m_ShadowViewport);
        CommandList->RSSetScissorRects(1, &m_ShadowScissorRect);
        // Shadow Pass 缁樺埗鍦烘櫙鍒版繁搴﹀浘
        DirectX::XMVECTOR lightDirVec = XMLoadFloat3(&LightConstantInstance.LightDirection);
        lightDirVec = DirectX::XMVector3Normalize(lightDirVec);
        DirectX::XMVECTOR lightPos = DirectX::XMVectorScale(lightDirVec, 20.0f);
        DirectX::XMVECTOR targetPos = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f); // 鐪嬪悜鍘熺偣
        DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(
            DirectX::XMVectorSubtract(targetPos, lightPos)
        );
        float dot = fabsf(DirectX::XMVectorGetX(DirectX::XMVector3Dot(forward, up)));
        if (dot > 0.99f)
        {
            // 杩欑鎯呭喌涓嬶紝寮哄埗鎶?Z 杞村綋浣?Up 鍚戦噺
            up = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        }
        DirectX::XMMATRIX lightView = DirectX::XMMatrixLookAtLH(lightPos, targetPos, up);
        // 姝ｄ氦鎶曞奖鑼冨洿 (瑕嗙洊浣犵殑鍦烘櫙澶у皬)
        // 姣斿鍦烘櫙鏄?20x20 绫筹紝杩欓噷灏辫瀹戒竴鐐?
        DirectX::XMMATRIX lightProj = DirectX::XMMatrixOrthographicLH(20.0f, 20.0f, 1.0f, 50.0f); // 瀹? 楂? 杩? 杩?
        DirectX::XMMATRIX lightViewProj = lightView * lightProj;
        CommandList->SetGraphicsRootSignature(RootSignature.Get());
        auto MainCameraPos = MainCamera.GetPosition();
        LightConstantInstance.CameraPosition = { MainCameraPos.x, MainCameraPos.y,MainCameraPos.z };
        // 灏嗘暟鎹嫹璐濆埌 Map 濂界殑鍐呭瓨涓?
        if (LightConstantBufferMappedData)
        {
            memcpy(LightConstantBufferMappedData, &LightConstantInstance, sizeof(LightConstants));
        }
        // 鏇挎崲涓篊BV
        //CommandList->SetGraphicsRootDescriptorTable(1, LightCbvGpuHandle);
        CommandList->SetGraphicsRootConstantBufferView(1, CurrFrameResource.LightConstantBuffer->GetGPUVirtualAddress());
        // XMStoreFloat4x4(&shadowConstants.WorldViewProj, XMMatrixTranspose(MVP));
        XMStoreFloat4x4(&LightConstantInstance.LightViewProj, XMMatrixTranspose(lightViewProj));
        const UINT ObjConstantBufferSize = (sizeof(ObjectConstants) + 255) & ~255;
        for (int i = 0; i < MeshList.size(); ++i)
        {
            auto MeshElement = MeshList[i];
            if (!MeshElement->IsVisible()) continue;
            // 璁＄畻 MVP = World * LightView * LightProj
            // 鏇存柊甯搁噺缂撳啿鍖?
            auto MVPMatrix = MeshElement->CalMVPMatrix(MainCamera.CalViewProjMatrix());
            auto M_Matrix = MeshElement->GetWorldMatrix();
            ObjectConstants objConstants;
            DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
            DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(M_Matrix));
            memcpy(CurrFrameResource.ObjectConstantBufferMappedData + i * ObjConstantBufferSize, &objConstants, sizeof(objConstants));
        	CommandList->SetGraphicsRootConstantBufferView(0, CurrFrameResource.ObjectConstantBuffer->GetGPUVirtualAddress() + i * ObjConstantBufferSize);
            // 缁樺埗
            CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            auto VBView = MeshElement->GetVertexBufferView();
            CommandList->IASetVertexBuffers(0, 1, &VBView);
            auto IBView = MeshElement->GetIndexBufferView();
            CommandList->IASetIndexBuffer(&IBView);
            CommandList->DrawIndexedInstanced(MeshElement->GetIndexCount(), 1, 0, 0, 0);
        }
    };

	IRenderPass* shadowMaskPass = m_PassManager.AddPass<ShadowMaskPass>();
    shadowMaskPass->SetRootSignature(RootSignature.Get());
    shadowMaskPass->AddDependency(m_ShadowMap.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	shadowMaskPass->AddDependency(m_SceneDepth.get(), D3D12_RESOURCE_STATE_DEPTH_READ);
    shadowMaskPass->AddDependency(m_ShadowMask.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    shadowMaskPass->Execute = [this](ID3D12GraphicsCommandList* CommandList)
        {

            auto& CurrFrameResource = FrameResources[CurrentFrameResourceIndex];

            m_ShadowMask->Clear(CommandList);
            // 鎵嬪姩缁戝畾 RTV + DSV锛屼笉浣跨敤 SetAsRenderTarget 鍥犱负瀹冧細寮哄埗灏?depth 杞负 DEPTH_WRITE
            // ShadowMaskPass 鐨?PSO 浣跨敤 DepthWriteMask=ZERO锛屾墍浠?DEPTH_READ 鏄纭殑鐘舵€?
            D3D12_CPU_DESCRIPTOR_HANDLE shadowMaskRtv = m_ShadowMask->GetRTV();
            D3D12_CPU_DESCRIPTOR_HANDLE sceneDepthDsv = m_SceneDepth->GetDSV();
            CommandList->OMSetRenderTargets(1, &shadowMaskRtv, FALSE, &sceneDepthDsv);

            CommandList->RSSetViewports(1, &ScreenViewport);
            CommandList->RSSetScissorRects(1, &ScissorRect);

            // 缁戝畾鎻忚堪绗﹀爢
            ID3D12DescriptorHeap* descriptorHeaps[] = {
                DescriptorAllocatorManager::GetInstance().GetCBV_SRV_UAV_Heap()
            };
            CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

            CommandList->SetGraphicsRootSignature(RootSignature.Get());
            CommandList->SetGraphicsRootDescriptorTable(3, m_ShadowMap->GetSRV_G()); // 缁戝畾 shadow map SRV

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
                if (!MeshElement->IsVisible()) continue;
                {
                    // 鏇存柊甯搁噺缂撳啿鍖?
                    auto MVPMatrix = MeshElement->CalMVPMatrix(MainCamera.CalViewProjMatrix());
                    auto M_Matrix = MeshElement->GetWorldMatrix();
                    ObjectConstants objConstants;
                    DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
                    DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(M_Matrix));
                    // MeshElement->UpdateObjectConstantBuffer(objConstants);

                    memcpy(CurrFrameResource.ObjectConstantBufferMappedData + i * ObjConstantBufferSize, &objConstants, sizeof(objConstants));
                }

                // 缁戝畾 CBV
                CommandList->SetGraphicsRootConstantBufferView(0, CurrFrameResource.ObjectConstantBuffer->GetGPUVirtualAddress() + i * ObjConstantBufferSize);

                CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                auto VertexBufferView = MeshElement->GetVertexBufferView();
                CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
                auto IndexBufferView = MeshElement->GetIndexBufferView();
                CommandList->IASetIndexBuffer(&IndexBufferView);

                CommandList->DrawIndexedInstanced(MeshElement->GetIndexCount(), 1, 0, 0, 0);
            }

        };


	IRenderPass* basePass = m_PassManager.AddPass<BasePass>();
	basePass->SetRootSignature(RootSignature.Get());
    basePass->AddDependency(m_GBuffer0.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    basePass->AddDependency(m_GBuffer1.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    basePass->AddDependency(m_GBuffer2.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    basePass->AddDependency(m_SceneDepth.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

    basePass->Execute = [this](ID3D12GraphicsCommandList* CommandList)
        {
            auto& CurrFrameResource = FrameResources[CurrentFrameResourceIndex];
            D3D12_CPU_DESCRIPTOR_HANDLE rtvs[] = {
                m_GBuffer0->GetRTV(),
                m_GBuffer1->GetRTV(),
                m_GBuffer2->GetRTV()
            };
            auto dsv = m_SceneDepth->GetDSV();
            m_GBuffer0->Clear(CommandList);
            m_GBuffer1->Clear(CommandList);
            m_GBuffer2->Clear(CommandList);

            CommandList->OMSetRenderTargets(3, rtvs, FALSE, &dsv);
            CommandList->RSSetViewports(1, &ScreenViewport);
            CommandList->RSSetScissorRects(1, &ScissorRect);

            auto MainCameraPos = MainCamera.GetPosition();
            LightConstantInstance.CameraPosition = { MainCameraPos.x, MainCameraPos.y,MainCameraPos.z };
            DirectX::XMMATRIX ViewProj = MainCamera.CalViewProjMatrix();
            DirectX::XMVECTOR det;
            DirectX::XMMATRIX InvViewProj = XMMatrixInverse(&det, ViewProj);
            XMStoreFloat4x4(&LightConstantInstance.CameraInvViewProj, XMMatrixTranspose(InvViewProj));

            if (CurrFrameResource.LightConstantBufferMappedData)
            {
                memcpy(CurrFrameResource.LightConstantBufferMappedData, &LightConstantInstance, sizeof(LightConstants));
            }
            CommandList->SetGraphicsRootConstantBufferView(1, CurrFrameResource.LightConstantBuffer->GetGPUVirtualAddress());
            const UINT ObjConstantBufferSize = (sizeof(ObjectConstants) + 255) & ~255;
            const UINT MatConstantBufferSize = (sizeof(MaterialConstants) + 255) & ~255;
            for (int i = 0; i < MeshList.size(); ++i)
            {

                auto MeshElement = MeshList[i];
                if (!MeshElement->IsVisible()) continue;
                {
                    auto MVPMatrix = MeshElement->CalMVPMatrix(MainCamera.CalViewProjMatrix());
                    auto M_Matrix = MeshElement->GetWorldMatrix();
                    ObjectConstants objConstants;
                    DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(MVPMatrix));
                    DirectX::XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(M_Matrix));
                    // 鏇存柊甯ц祫婧愮殑constantbuffer+idx鍋忕Щ鐨勬暟鎹?
                    memcpy(CurrFrameResource.ObjectConstantBufferMappedData + i * ObjConstantBufferSize, &objConstants, sizeof(objConstants));
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
		};

	IRenderPass* deferredLightPass = m_PassManager.AddPass<DeferredLightPass>();
    deferredLightPass->AddDependency(m_GBuffer0.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    deferredLightPass->AddDependency(m_GBuffer1.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    deferredLightPass->AddDependency(m_GBuffer2.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    // GBUFFER3 TO DO
    deferredLightPass->AddDependency(m_SceneDepth.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    deferredLightPass->AddDependency(m_ShadowMap.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    deferredLightPass->AddDependency(m_ShadowMask.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    deferredLightPass->AddDependency(m_IrradianceMap.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    deferredLightPass->AddDependency(m_PrefilterMap.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    deferredLightPass->AddDependency(m_BrdfLUTTexture.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    deferredLightPass->Execute = [this](ID3D12GraphicsCommandList* CommandList)
        {
            auto& CurrFrameResource = FrameResources[CurrentFrameResourceIndex];

            CD3DX12_RESOURCE_BARRIER Barrier_P2RT = CD3DX12_RESOURCE_BARRIER::Transition(
                RenderTargets[CurrentFrameIdx].Get(),
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET
            );

            //CommandList->IASetVertexBuffers(0, 0, nullptr);
            //CommandList->IASetIndexBuffer(nullptr);

            CommandList->ResourceBarrier(1, &Barrier_P2RT);
            D3D12_CPU_DESCRIPTOR_HANDLE CPU_RTV_Handle = RtvHeap->GetCPUDescriptorHandleForHeapStart();
            CPU_RTV_Handle.ptr += CurrentFrameIdx * RtvDescriptorSize;
            const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f }; // 娣辫摑鑹茶儗鏅?
            CommandList->ClearRenderTargetView(CPU_RTV_Handle, clearColor, 0, nullptr);
            CommandList->OMSetRenderTargets(1, &CPU_RTV_Handle, FALSE, nullptr);

            // 璁剧疆鎻忚堪绗﹀爢锛堝繀椤诲湪缁戝畾 SRV 涔嬪墠锛?
            ID3D12DescriptorHeap* descriptorHeaps[] = {
                DescriptorAllocatorManager::GetInstance().GetCBV_SRV_UAV_Heap()
            };
            CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

            CommandList->RSSetViewports(1, &ScreenViewport);
            CommandList->RSSetScissorRects(1, &ScissorRect);
            {
                //Light Constants
                auto MainCameraPos = MainCamera.GetPosition();
                LightConstantInstance.CameraPosition = { MainCameraPos.x, MainCameraPos.y,MainCameraPos.z };
                DirectX::XMMATRIX ViewProj = MainCamera.CalViewProjMatrix();
                DirectX::XMVECTOR det;
                DirectX::XMMATRIX InvViewProj = XMMatrixInverse(&det, ViewProj);
                XMStoreFloat4x4(&LightConstantInstance.CameraInvViewProj, XMMatrixTranspose(InvViewProj));
                if (CurrFrameResource.LightConstantBufferMappedData)
                {
                    memcpy(CurrFrameResource.LightConstantBufferMappedData, &LightConstantInstance, sizeof(LightConstants));
                }
                CommandList->SetGraphicsRootConstantBufferView(0, CurrFrameResource.LightConstantBuffer->GetGPUVirtualAddress());
            }
            {
                m_GBuffer0->BindSRV_Graphics(CommandList, 1);
                m_GBuffer1->BindSRV_Graphics(CommandList, 2);
                m_GBuffer2->BindSRV_Graphics(CommandList, 3);
                m_ShadowMask->BindSRV_Graphics(CommandList, 5);
                m_IrradianceMap->BindSRV_Graphics(CommandList, 6);
                m_PrefilterMap->BindSRV_Graphics(CommandList, 7);
                m_BrdfLUTTexture->BindSRV_Graphics(CommandList, 8);
                m_SceneDepth->BindSRV_Graphics(CommandList, 9);
            }
            CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            CommandList->DrawInstanced(3, 1, 0, 0);

        };

	IRenderPass* skyPass = m_PassManager.AddPass<SkyboxPass>();
	skyPass->SetRootSignature(RootSignature.Get());
    skyPass->AddDependency(m_EnvCubeMap.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    skyPass->AddDependency(m_SceneDepth.get(), D3D12_RESOURCE_STATE_DEPTH_READ);
    skyPass->Execute = [this](ID3D12GraphicsCommandList* CommandList)
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

            // Skybox 鐢ㄥぇ buffer 鏈€鍚庝竴涓Ы浣?
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

    m_PassManager.InitAllPasses(Device::GetInstance().GetD3DDevice());
}

void DXRender::InitTextures()
{
    //depth
    //m_SceneDepth = std::make_shared<Texture>("SceneDepth");
    //m_SceneDepth->Create(
    //    CommandList.Get(),
    //    TextureDesc::CreateDepth(Width, Height, true)
    //);

    // shadow
    m_ShadowMap = std::make_shared<Texture>("ShadowMap");
    // 鍒涘缓 ShadowMap (DSV + SRV)
    m_ShadowMap->Create(
        CommandList.Get(),
        TextureDesc::CreateDepth(ShadowMapSize, ShadowMapSize, true) //SRV (缁?MainPass 閲囨牱)
    );

    // 鍒濆鍖栬鍙?(Shadow Pass 涓撶敤)
    m_ShadowViewport = { 0.0f, 0.0f, (float)ShadowMapSize, (float)ShadowMapSize, 0.0f, 1.0f };
    m_ShadowScissorRect = { 0, 0, (LONG)ShadowMapSize, (LONG)ShadowMapSize };

    m_ShadowMask = std::make_shared<Texture>("ShadowMask");
    // 鍒涘缓 ShadowMask (RTV + SRV, LDR鏍煎紡)
    m_ShadowMask->Create(
        CommandList.Get(),
        TextureDesc::Create2D(Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM, TextureViewFlags::RTV | TextureViewFlags::SRV)
    );


    //GBUFFER
    m_GBuffer0 = std::make_shared<Texture>("GBuffer0");
    m_GBuffer1 = std::make_shared<Texture>("GBuffer1");
    m_GBuffer2 = std::make_shared<Texture>("GBuffer2");

    TextureDesc GBuffer0Desc;
    GBuffer0Desc.Width = Width;
    GBuffer0Desc.Height = Height;
    GBuffer0Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    GBuffer0Desc.ViewFlags = TextureViewFlags::RTV | TextureViewFlags::SRV | TextureViewFlags::UAV;
    m_GBuffer0->Create(CommandList.Get(), GBuffer0Desc);

    TextureDesc GBuffer1Desc;
    GBuffer1Desc.Width = Width;
    GBuffer1Desc.Height = Height;
    GBuffer1Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    GBuffer1Desc.ViewFlags = TextureViewFlags::RTV | TextureViewFlags::SRV | TextureViewFlags::UAV;
    m_GBuffer1->Create(CommandList.Get(), GBuffer1Desc);

    TextureDesc GBuffer2Desc;
    GBuffer2Desc.Width = Width;
    GBuffer2Desc.Height = Height;
    GBuffer2Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    GBuffer2Desc.ViewFlags = TextureViewFlags::RTV | TextureViewFlags::SRV | TextureViewFlags::UAV;
    m_GBuffer2->Create(CommandList.Get(), GBuffer2Desc);

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
    // 璁＄畻 CPU 鍙ユ焺 (鐢ㄤ簬鍒涘缓璧勬簮)
    //CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(
    //    ConstantBufferViewHeap->GetCPUDescriptorHandleForHeapStart(),
    //    CurrentSrvHeapIndex,
    //    DescriptorSize);

    //// 璁＄畻 GPU 鍙ユ焺 (鐢ㄤ簬缁戝畾 Shader)
    //CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(
    //    ConstantBufferViewHeap->GetGPUDescriptorHandleForHeapStart(),
    //    CurrentSrvHeapIndex,
    //    DescriptorSize);

	unsigned int RetIdx = CurrentSrvHeapIndex;

    CurrentSrvHeapIndex++; // 鎸囬拡鍚庣Щ

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

    // 姣忎釜 Pass 涓€濂楃嫭绔嬬殑 Allocator + CmdList锛堝绾跨▼褰曞埗鐢級
    for (int i = 0; i < PASS_COUNT; ++i)
    {
        ThrowIfFailed(device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&PassCmdAllocators[i])));
        ThrowIfFailed(device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            PassCmdAllocators[i].Get(), nullptr,
            IID_PPV_ARGS(&PassCmdLists[i])));
        // 鍒涘缓鍑烘潵鏄?open 鐘舵€侊紝鍏?Close 绛夊緟 Draw 閲屽啀 Reset
        PassCmdLists[i]->Close();
    }

    // Light Constant Buffer
    const UINT LightConstantBufferSize = (sizeof(LightConstants) + 255) & ~255;

    // 鍒涘缓涓婁紶鍫嗙殑甯搁噺缂撳啿璧勬簮
    CD3DX12_HEAP_PROPERTIES LightHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(LightConstantBufferSize);
    ThrowIfFailed(device->CreateCommittedResource(&LightHeapProps, D3D12_HEAP_FLAG_NONE, &BufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&LightConstantBuffer)));
    // 2) 鏄犲皠寰楀埌 CPU 鍙啓鎸囬拡
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
