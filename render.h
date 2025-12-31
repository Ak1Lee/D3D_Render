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
#include <functional>

constexpr unsigned int FrameBufferCount = 2;
inline unsigned int Width = 800;
inline unsigned int Height = 600;


struct RenderPass
{
    std::string Name;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSig; // 如果不同 Pass RootSig 不同

    // 视口和裁剪矩形
    D3D12_VIEWPORT Viewport;
    D3D12_RECT ScissorRect;

    std::function<void(ID3D12GraphicsCommandList*)> Execute;
};

class Texture
{

public:
	Texture(const std::string& InName) : Name(InName) {}
    ~Texture(){ Release(); }


    void LoadFromFile(ID3D12GraphicsCommandList* CmdList, std::string Filename, bool isRGB = false);

    void LoadHDRFromFile(ID3D12GraphicsCommandList* CmdList,std::string Filename);

    void Release();

	ID3D12Resource* GetResource() { return Resource.Get(); }

	int GetDescriptorHeapIndex()const { return Handle.Index; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle() const { return Handle.CpuHandle; }

	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const { return Handle.GpuHandle; }

private:
    std::string Name;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource;

    Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap;

    DescriptorAllocation Handle;

    int Width = 0;
    int Height = 0;

    DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    void CreateTextureResource(ID3D12Device* device,
        ID3D12GraphicsCommandList* CmdList,
        const void* InData,
        int Width, int Height,
        DXGI_FORMAT Format,             // 格式：R8G8B8A8 或 R32G32B32A32
        int PixelByteSize,              // 像素字节大小：4 (LDR) 或 16 (HDR)
        Microsoft::WRL::ComPtr<ID3D12Resource>& OutResource,
        Microsoft::WRL::ComPtr<ID3D12Resource>& OutUploadHeap);

};

class GraphicsPSOBuilder
{
public:
    GraphicsPSOBuilder(ID3D12RootSignature* rootSig)
    {
        // 1. 在这里把所有乱七八糟的默认值都填好！
        m_Desc = {};
        m_Desc.pRootSignature = rootSig;
        m_Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        m_Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        m_Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        m_Desc.SampleMask = UINT_MAX;
        m_Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        m_Desc.SampleDesc.Count = 1;
        m_Desc.NumRenderTargets = 1;
        m_Desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        m_Desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        // Input Layout 默认用标准的
        m_Desc.InputLayout = { StandardVertexInputLayout, _countof(StandardVertexInputLayout) };
    }

    // 链式调用方法
    GraphicsPSOBuilder& SetShaders(const std::wstring& vsName, const std::wstring& psName);


    GraphicsPSOBuilder& SetRTVFormats(const std::vector<DXGI_FORMAT>& formats)
    {
        m_Desc.NumRenderTargets = (UINT)formats.size();
        for (int i = 0; i < formats.size(); ++i) m_Desc.RTVFormats[i] = formats[i];
        return *this;
    }

    // 专门给 Shadow Pass 用
    GraphicsPSOBuilder& SetDepthOnly(DXGI_FORMAT dsvFormat)
    {
        m_Desc.NumRenderTargets = 0;
        m_Desc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
        m_Desc.DSVFormat = dsvFormat;
        return *this;
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> Build(ID3D12Device* device)
    {
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
        device->CreateGraphicsPipelineState(&m_Desc, IID_PPV_ARGS(&pso));
        return pso;
    }

private:
    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_Desc;
};

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
	unsigned int GetRtvDescriptorSize() { return RtvDescriptorSize; }
    unsigned int GetDsvDescriptorSize() { return DsvDescriptorSize; }
    unsigned int GetSrvUavDescriptorSize() { return SrvUavDescriptorSize; }

    void InitShadowMap();
	void InitShadowPSO();

	void InitPasses();

    void InitShadowMaskTexture();
	void InitShadowMaskPSO();

    void InitZPrepassPSO();

    //csTest
    //todo 
	void InitComputeRootSignature();
    void ComputeCubemap();
	void InitEnvCubeMap();

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

	int CurrentSrvHeapIndex = 0;
	const unsigned int MAX_HEAP_SIZE = 100;
    DescriptorAllocation AllocateDescriptorHandle(unsigned int DescriptorSize);

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

    // Maeterial Constant Buffer View b2
    Microsoft::WRL::ComPtr<ID3D12Resource> MaterialConstantBuffer;
	UINT8* MaterialConstantBufferMappedData = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE MaterialCbvCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE MaterialCbvGpuHandle;
	MaterialConstants MaterialConstantInstance;
	static const UINT MaterialCbvHeapIndex = 18;


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

    //imgui
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ImguiSrvHeap = nullptr;

    friend class Material;
	friend class Texture;


    // ShadowMap
    Microsoft::WRL::ComPtr<ID3D12Resource> ShadowMap;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ShadowDSVHeap;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> ShadowPSO;

    D3D12_CPU_DESCRIPTOR_HANDLE ShadowDSVHandle;
    DescriptorAllocation ShadowSRVHandle;
    const UINT ShadowMapSize = 2048;
    D3D12_VIEWPORT m_ShadowViewport;
    D3D12_RECT m_ShadowScissorRect;

    // ShadowMask
    Microsoft::WRL::ComPtr<ID3D12Resource> ShadowMaskTexture;
    CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMaskRTVHandle;
	DescriptorAllocation ShadowMaskSRVHandle;



    // Pass

    RenderPass ShadowPass;
	RenderPass MainPass;
    RenderPass ShadowMaskPass;
	RenderPass ZPrePass;



    std::vector<RenderPass> RenderPasses;

	//compute shader
    // CD3DX12_DESCRIPTOR_RANGE CSSrvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    Microsoft::WRL::ComPtr<ID3D12RootSignature> ComputeRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> ComputePipelineState;

    Texture HDRTex = Texture( "puresky" );
    Microsoft::WRL::ComPtr<ID3D12Resource> EnvCubeMap;
    DescriptorAllocation EnvCubeUAVHandle;
    DescriptorAllocation EnvCubeSRVHandle;

};

