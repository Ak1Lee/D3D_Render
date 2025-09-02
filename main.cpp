#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tchar.h>
#include <wrl.h>

#include <dxgi1_6.h>
#include <DirectXMath.h>


//for d3d12
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>

//linker
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#include "d3dx12.h"
#include <iostream>
#include <comdef.h>
#include <fstream>
#include "Timer.h"

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}


class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

    std::wstring ToString()const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

// 替换 std::to_wstring(LineNumber) 为如下实现
#include <sstream>

std::wstring DxException::ToString() const
{
    // 获取错误码的字符串描述
    _com_error err(ErrorCode);
    std::wstring msg = err.ErrorMessage();

    std::wstringstream ss;
    ss << LineNumber;

    return FunctionName + L" failed in " + Filename + L"; line " + ss.str() + L"; error: " + msg;
}
// 在 DxException 类定义之后添加这些实现




DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
    ErrorCode(hr),
    FunctionName(functionName),
    Filename(filename),
    LineNumber(lineNumber)
{
}

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif


struct Vertex
{
    DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT4 color;
};

//using namespace std;
//
//int main() {
//	cout << "Hello, World!" << endl;
//	return 0;
//}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 注册窗口类
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MyWindowClass";
    RegisterClass(&wc);

    // 创建窗口
    HWND hwnd = CreateWindow(
        L"MyWindowClass", L"My DX12 App",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hwnd, nCmdShow);

    //AllocConsole(); // 申请控制台
    //freopen("CONOUT$", "w", stdout); // 绑定 std::cout 到控制台
    //freopen("CONOUT$", "w", stderr);

    std::cout << "Hello Console!" << std::endl;

    // 变量定义
    // -----------------------------------------------//

    const unsigned int FrameBufferCount = 2;

    Microsoft::WRL::ComPtr<IDXGIFactory4> DxgiFactory;
	Microsoft::WRL::ComPtr<ID3D12Device> D3DDevice;
    Microsoft::WRL::ComPtr<ID3D12Fence> Fence;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CommandList;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineState;

    



	Microsoft::WRL::ComPtr<IDXGISwapChain1> SwapChain;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> SwapChain3;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RtvHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> RenderTargets[FrameBufferCount];
	Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature;
    Microsoft::WRL::ComPtr<ID3DBlob> Signature;
    // Shader Compile
    Microsoft::WRL::ComPtr<ID3DBlob> VS;
    Microsoft::WRL::ComPtr<ID3DBlob> PS;

	unsigned int RtvDescriptorSize = 0;
	unsigned int DsvDescriptorSize = 0;
    unsigned int SrvUavDescriptorSize = 0;

	
	unsigned int CurrentFrameIdx = 0;

	unsigned int Width = 800;
	unsigned int Height = 600;
	UINT64 FenceValue = 0;
    D3D12_VIEWPORT ScreenViewport;
    D3D12_RECT ScissorRect;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};

	Timer timer;

    float FPS = 10.0;
	float FrameDeltaTime = 1.0f / FPS;

    int FrameCount = 0;

    // -----------------------------------------------//



    // 创建 device
    {
#if defined(DEBUG) || defined(_DEBUG)
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
			debugController->EnableDebugLayer();

            std::cout << " Debug Enable Success!" << std::endl;
		}
#endif

    }
    {
        ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&DxgiFactory)));

        // 创建device
        HRESULT HardwareCreateResult = D3D12CreateDevice(
            nullptr,             // 默认适配器
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&D3DDevice)
		);

        if(HardwareCreateResult == E_NOINTERFACE)
        {
            std::cout << "Direct3D 12 is not supported on this device" << std::endl;
            return -1;

            std::cout << " Create Software Simulation" << std::endl;
            Microsoft::WRL::ComPtr<IDXGIAdapter> WARPAdapter;
            ThrowIfFailed(DxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&WARPAdapter)));
        }
        else
        {
            ThrowIfFailed(HardwareCreateResult);
            std::cout << " D3D12 Create Device Success!" << std::endl;
		}

        // fence
        ThrowIfFailed(D3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&Fence)));
        // 

		//句柄大小?

        //创建命令队列
		D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
		QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		ThrowIfFailed(D3DDevice->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&CommandQueue)));
		ThrowIfFailed(D3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CommandAllocator)));

        ThrowIfFailed(D3DDevice->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,CommandAllocator.Get(),nullptr,IID_PPV_ARGS(CommandList.GetAddressOf())));

        CommandList->Close();


        // 交换链
		DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {};
        SwapChainDesc.BufferCount = FrameBufferCount;
		SwapChainDesc.Width = Width;
		SwapChainDesc.Height = Height;
		SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		SwapChainDesc.SampleDesc.Count = 1;


        ThrowIfFailed(DxgiFactory->CreateSwapChainForHwnd(
            CommandQueue.Get(),
            hwnd,
            &SwapChainDesc,
            nullptr,
            nullptr,
            SwapChain.GetAddressOf()
        ));

        ThrowIfFailed(SwapChain.As(&SwapChain3));

		// RenderTargetView Heap 和 RenderTargetView 描述符大小
		D3D12_DESCRIPTOR_HEAP_DESC RtvHeapDesc = {};
        RtvHeapDesc.NumDescriptors = FrameBufferCount;
		RtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		RtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ThrowIfFailed(D3DDevice->CreateDescriptorHeap(
            &RtvHeapDesc, IID_PPV_ARGS(&RtvHeap)
        ));

        // RTV Create 
		CD3DX12_CPU_DESCRIPTOR_HANDLE RtvHandle(RtvHeap->GetCPUDescriptorHandleForHeapStart());
        for(UINT i=0;i< FrameBufferCount;++i)
        {
            ThrowIfFailed(SwapChain3->GetBuffer(i,IID_PPV_ARGS(&RenderTargets[i])));
            D3DDevice->CreateRenderTargetView(RenderTargets[i].Get(),nullptr,RtvHandle);
            RtvHandle.Offset(1, D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		}

		// RootSignature
		CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc;
        // Serializes a root signature version 1.0 that can be passed to ID3D12Device::CreateRootSignature.
		RootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		ThrowIfFailed(D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &Signature, nullptr));

        ThrowIfFailed(D3DDevice->CreateRootSignature(0, Signature->GetBufferPointer(), Signature->GetBufferSize(), IID_PPV_ARGS(&RootSignature)));
    }
    


#if defined(_DEBUG)
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif
	TCHAR shaderPath[] = _T("Shaders.hlsl");
    ThrowIfFailed(D3DCompileFromFile(shaderPath, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &VS, nullptr));
    ThrowIfFailed(D3DCompileFromFile(shaderPath, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &PS, nullptr));

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
	D3DDevice->CreateGraphicsPipelineState(&PsoDesc, IID_PPV_ARGS(&PipelineState));


    //triangle
    Vertex TriangleVertices[] = {
        {{0.0f, 0.25f * Height / Width, 0.0f }, {1.0f, 0.0f, 0.0f, 1.0f}},
        { { 0.25f, -0.25f * Height / Width, 0.0f }, {0.0f, 1.0f, 0.0f, 1.0f} },
        { { -0.25f, -0.25f * Height / Width, 0.0f }, {0.0f, 0.0f, 1.0f, 1.0f}}
    };
	const unsigned int TriangleVertexBufferSize = sizeof(TriangleVertices);

    CD3DX12_HEAP_PROPERTIES UploadProp(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(TriangleVertexBufferSize);
    ThrowIfFailed(D3DDevice->CreateCommittedResource(
        &UploadProp,
        D3D12_HEAP_FLAG_NONE,
        &ResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&VertexBuffer)
    ))
    ;
    UINT8* VertexDataBegin = nullptr;
    CD3DX12_RANGE ReadRange(0, 0);
    VertexBuffer->Map(0, &ReadRange, reinterpret_cast<void**>(&VertexDataBegin));
    memcpy(VertexDataBegin, TriangleVertices, TriangleVertexBufferSize);
    VertexBuffer->Unmap(0, nullptr);

    VertexBufferView.BufferLocation = VertexBuffer->GetGPUVirtualAddress();
    VertexBufferView.StrideInBytes = sizeof(Vertex);
    VertexBufferView.SizeInBytes = TriangleVertexBufferSize;

	// VIEWPORT and SCISSOR
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

 //   auto VertexBufferBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
 //       VertexBuffer.Get(),
 //       D3D12_RESOURCE_STATE_COMMON,
 //       D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	//);
 //   CommandList->ResourceBarrier(1, &VertexBufferBarrier);

	VertexBuffer->SetName(L"Vertex Buffer Resource");
    CommandList->SetName(L"MainCommandList");
    CommandQueue->SetName(L"MainCommandQueue");
    for (UINT i = 0; i < FrameBufferCount; ++i)
    {
        RenderTargets[i]->SetName((L"RenderTarget_" + std::to_wstring(i)).c_str());
    }
    float AccumTime = 0.0f;

    MSG msg = {};
    while (GetMessage(&msg, nullptr,0,0))
    {
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        // std::cout<< "Hello, World!" << std::endl;

        float Deltatime = timer.GetDeltaTimeAndUpdateLastTime();
		AccumTime += Deltatime;
        if(AccumTime < FrameDeltaTime)
        {
            float SleepTime = (FrameDeltaTime - AccumTime) * 1000.0f;
            Sleep((DWORD)(SleepTime));
		}
		
		Deltatime += timer.GetDeltaTimeAndUpdateLastTime();
        FrameCount++;
        printf("Start Render, DeltaTime : %f,Frame Count : %d \n", Deltatime, FrameCount);
        //std::cout << "Start Render, DeltaTime : " << Deltatime << ", Frame Count : " << FrameCount << std::endl;
        AccumTime -= FrameDeltaTime;
        // draw
        {

            ThrowIfFailed(CommandAllocator->Reset());
            ThrowIfFailed(CommandList->Reset(CommandAllocator.Get(), PipelineState.Get()));


			CommandList->SetGraphicsRootSignature(RootSignature.Get());
			CommandList->RSSetViewports(1, &ScreenViewport);
			CommandList->RSSetScissorRects(1, &ScissorRect);



            CD3DX12_RESOURCE_BARRIER Barrier_P2RT = CD3DX12_RESOURCE_BARRIER::Transition(
                RenderTargets[CurrentFrameIdx].Get(),
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET
            );
            CommandList->ResourceBarrier(1, &Barrier_P2RT);
			D3D12_CPU_DESCRIPTOR_HANDLE CPU_RTV_Handle = RtvHeap->GetCPUDescriptorHandleForHeapStart();
            //CD3DX12_CPU_DESCRIPTOR_HANDLE
			CPU_RTV_Handle.ptr += CurrentFrameIdx * D3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			CommandList->OMSetRenderTargets(1, &CPU_RTV_Handle, FALSE, nullptr);

            // 添加清除渲染目标
            const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f }; // 深蓝色背景
            CommandList->ClearRenderTargetView(CPU_RTV_Handle, clearColor, 0, nullptr);


			CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			CommandList->IASetVertexBuffers(0, 1, &VertexBufferView);
			CommandList->DrawInstanced(3, 1, 0, 0);

            CD3DX12_RESOURCE_BARRIER Barrier_RT2P = CD3DX12_RESOURCE_BARRIER::Transition(
                RenderTargets[CurrentFrameIdx].Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT
            );
            CommandList->ResourceBarrier(1, &Barrier_RT2P);

			ThrowIfFailed(CommandList->Close());

			ID3D12CommandList* CmdLists[] = { CommandList.Get() };
            CommandQueue->ExecuteCommandLists(_countof(CmdLists), CmdLists);
			ThrowIfFailed(SwapChain3->Present(1, 0));

			const UINT64 CurrentFenceValue = FenceValue;
			CommandQueue->Signal(Fence.Get(), CurrentFenceValue);
            FenceValue++;

            if (Fence->GetCompletedValue() < CurrentFenceValue)
            {
                HANDLE EventHandle = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
                Fence->SetEventOnCompletion(CurrentFenceValue, EventHandle);
                WaitForSingleObject(EventHandle, INFINITE);
                CloseHandle(EventHandle);
            }
			CurrentFrameIdx = SwapChain3->GetCurrentBackBufferIndex();
			//CommandAllocator->Reset();
			//CommandList->Reset(CommandAllocator.Get(), PipelineState.Get());



        }

    }

    return (int)msg.wParam;


}

