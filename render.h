#pragma once
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include "d3dx12.h"
#include <windows.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <vector>
#include "dxUtils.h"
#include "MathHelper.h"
#include "DXDevice.h"
#include "Geometry.h"

#include "camera.h"

constexpr unsigned int FrameBufferCount = 2;
inline unsigned int Width = 800;
inline unsigned int Height = 600;

class DXRender
{
public:

    static DXRender& GetInstance();

    void Init(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);

    void InitDX(HWND hWnd);

    void ExecuteCommandAndWaitForComplete();

    void InitViewportAndScissor();

    void InitHandleSize();

    void InitCommand();

    void InitSwapChain(HWND hWnd);

    void InitRenderTargetHeapAndView();

    void CreateConstantBufferView();

    void InitRootSignature();

    void CompileShader();

    void InitInputLayout();

    void InitPSO();

	void InitMaterial();

    void CreateFence();

    void Draw();

    ~DXRender();

    Camera& GetMainCamera() { return MainCamera; }

private:
    DXRender();

    Camera MainCamera;

    Microsoft::WRL::ComPtr<ID3D12Fence> Fence;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CommandList;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAllocator;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineState;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> SwapChain1;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> SwapChain3;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> RtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> RenderTargets[FrameBufferCount];
    Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature;
    Microsoft::WRL::ComPtr<ID3DBlob> Signature;

    // Constant Buffer View Heap
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ConstantBufferViewHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> ObjectConstantBuffer;
    UINT8* ConstantBufferMappedData = nullptr;
    // Constant Buffer

	// Light Constant Buffer View
	Microsoft::WRL::ComPtr<ID3D12Resource> LightConstantBuffer;
	UINT8* LightConstantBufferMappedData = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE LightCbvCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE LightCbvGpuHandle;
	LightConstants LightConstantInstance;
    static const UINT LightCbvHeapIndex = 19;

	// DepthStencilBuffer
	Microsoft::WRL::ComPtr<ID3D12Resource> DepthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DsvHeap;
	void InitDepthStencilBuffer();
	// DepthStencilBuffer End

    // Shader Compile
    Microsoft::WRL::ComPtr<ID3DBlob> VS;
    Microsoft::WRL::ComPtr<ID3DBlob> PS;

    unsigned int RtvDescriptorSize = 0;
    unsigned int DsvDescriptorSize = 0;
    unsigned int SrvUavDescriptorSize = 0;

    unsigned int CurrentFrameIdx = 0;

    UINT64 FenceValue = 0;
    D3D12_VIEWPORT ScreenViewport;
    D3D12_RECT ScissorRect;

    //RenderableItem Triangle;

    //BoxMesh BoxShape;

	std::vector<MeshBase*> MeshList;

	MeshBase* PtrMesh = nullptr;

};

