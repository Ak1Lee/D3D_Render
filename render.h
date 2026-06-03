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

#include "DescriptorAllocator.h"
#include "Texture.h"
#include "camera.h"
#include "RenderPass.h"
#include <functional>
#include <assimp/Importer.hpp>

#include "RenderPasses/RenderPasses.h"
#include "Resource/ConstantBuffer.h"

constexpr unsigned int FrameBufferCount = 2;
inline unsigned int Width = 1920*0.75;
inline unsigned int Height = 1080 * 0.75;




class GraphicsPSOBuilder
{
public:
    GraphicsPSOBuilder(ID3D12RootSignature* rootSig)
    {
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

        // Input Layout 默认标准布局
        m_Desc.InputLayout = { StandardVertexInputLayout, _countof(StandardVertexInputLayout) };
    }

    // 格式化方法封装
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


// Pass 索引：和 Draw 里的提交顺序一致
enum EPassIndex : int
{
    PASS_ZPRE = 0,
    PASS_SHADOW = 1,
    PASS_SHADOWMASK = 2,
    PASS_MAIN = 3,
    PASS_SKY = 4,
    PASS_COUNT = 5
};

struct FrameResource
{
    ComPtr<ID3D12CommandAllocator> CmdAllocator;
    UINT64 FenceValue = 0;

    // 每Pass 独立的 Allocator + CmdList
    ComPtr<ID3D12CommandAllocator> PassCmdAllocators[PASS_COUNT];
    ComPtr<ID3D12GraphicsCommandList> PassCmdLists[PASS_COUNT];

    //LightCB
    ConstantBuffer<LightConstants> m_LightConstant;


	//ObjectCB
    ConstantBuffer<ObjectConstants> m_ObjectConstant;

	//MaterialCB
    ConstantBuffer<MaterialConstants> m_MaterialConstant;



    void Init(ID3D12Device* device, UINT maxObjectCount);

};

struct CascadeResource {
    ComPtr<ID3D12Resource> buffer;
	DescriptorHandle srvHandle;
    DescriptorHandle uavHandle;
    D3D12_RESOURCE_STATES m_CascadeState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    int spatialRes[3];      // 空间分辨率
    int probeSize;          // probe 边长
    int raysPerHemi;        // 每半球 ray 数
    int totalElements;      // 总 float4 数量
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

	void InitDeferredLightRootSignature();

    void CompileShader();

    void InitInputLayout();

    void InitPSO();

	void InitMaterial();

    void CreateFence();

    void PreDraw();

    void Draw();

    ~DXRender();

    Camera& GetMainCamera() { return MainCamera; }
	unsigned int GetRtvDescriptorSize() { return RtvDescriptorSize; }
    unsigned int GetDsvDescriptorSize() { return DsvDescriptorSize; }
    unsigned int GetSrvUavDescriptorSize() { return SrvUavDescriptorSize; }

	ID3D12GraphicsCommandList* GetCommandList() { return CommandList.Get(); }

    void InitPasses_new();
    void InitTextures();

    void InitGIContent();

	void InitComputeRootSignature();
    void ComputeCubemap();
	void InitIrradianceMapCompute();
	void ComputeIrradianceMap();
	void InitEnvCubeMapAndIrradianceMap();
	void InitIrradianceMap();
	void InitPrefilterRootSignature();
	void ComputePrefilterMap();
    void InitBRDFLUT();
	void ComputeBRDFLUT();

    void InitGBuffers();



    
    int CurrentFrameResourceIndex = 0;
    static const int FrameResourceNum = 3;
    FrameResource FrameResources[FrameResourceNum];

    // Getters for UI
    LightConstants& GetLightConstants() { return LightConstantInstance; }
    MaterialConstants& GetMaterialConstants() { return MaterialConstantInstance; }
    bool& GetMultiThreadRecordingFlag() { return bEnableMultiThreadRecord; }
    float GetLastRecordTimeMs() const { return m_LastRecordTimeMs; }
    const std::vector<MeshBase*>& GetMeshList() const { return MeshList; }


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
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_DeferredRootSignature;
    Microsoft::WRL::ComPtr<ID3DBlob> Signature;

	int CurrentSrvHeapIndex = 0;
	const unsigned int MAX_HEAP_SIZE = 100;
    DescriptorAllocation AllocateDescriptorHandle(unsigned int DescriptorSize);

    // Constant Buffer View Heap
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

	std::vector<MeshBase*> MeshList;

	MeshBase* PtrMesh = nullptr;

    //imgui
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ImguiSrvHeap = nullptr;

    friend class Material;
	friend class TextureTmp;
    friend class Texture;
    friend class MaterialManager;


    // ShadowMap
    //Microsoft::WRL::ComPtr<ID3D12Resource> ShadowMap;

    std::shared_ptr<Texture> m_ShadowMap = nullptr;
    std::shared_ptr<Texture> m_ShadowMask = nullptr;
    std::shared_ptr<Texture> m_SceneDepth = nullptr;
	std::shared_ptr<Texture> m_SceneColor = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ShadowDSVHeap;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> ShadowPSO;

    // D3D12_CPU_DESCRIPTOR_HANDLE ShadowDSVHandle;
    // DescriptorHandle ShadowSRVHandle;
    const UINT ShadowMapSize = 2048;
    D3D12_VIEWPORT m_ShadowViewport;
    D3D12_RECT m_ShadowScissorRect;

    // Pass
 //   RenderPass ShadowPass;
	//RenderPass MainPass;
 //   RenderPass ShadowMaskPass;
	//RenderPass ZPrePass;
 //   RenderPass SkyPass;

	ComPtr<ID3D12PipelineState> m_shadowPassPSO;
    ComPtr<ID3D12PipelineState> m_shadowMaskPassPSO;
    ComPtr<ID3D12PipelineState> m_zPrePassPSO;
    ComPtr<ID3D12PipelineState> m_skyPassPSO;
    ComPtr<ID3D12PipelineState> m_mainPassPSO;
    ComPtr<ID3D12PipelineState> m_bassPassPSO;
    ComPtr<ID3D12PipelineState> m_deferredLightPassPSO;


    bool bEnableMultiThreadRecord = false;
    float m_LastRecordTimeMs = 0.0f;



    //std::vector<RenderPass> RenderPasses;

	//compute shader
    Microsoft::WRL::ComPtr<ID3D12RootSignature> ComputeRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> ComputePipelineState;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> ComputeIrraRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> ComputeIrraPipelineState;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> ComputePrefilterRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> ComputePrefilterPipelineState;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> ComputeBRDFLUTRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> ComputeBRDFLUTPipelineState;

	std::shared_ptr<Texture> m_EnvCubeMap;
	std::shared_ptr<Texture> m_IrradianceMap;
	std::shared_ptr<Texture> m_HDRSkyTexture;
	std::shared_ptr<Texture> m_PrefilterMap;
	std::shared_ptr<Texture> m_BrdfLUTTexture;
	std::shared_ptr<Texture> m_DefaultWhiteTexture;
    //GBuffer
    // GBuffer0     :rgb:albedo     a: roughness
    std::shared_ptr<Texture> m_GBuffer0;
    // GBuffer1     :rgb:normal     a: metallic
    std::shared_ptr<Texture> m_GBuffer1;
	// GBuffer2     :rgb:emissive   a: occlusion
    std::shared_ptr<Texture> m_GBuffer2;


    // SKYBOX
    Box* SkyboxMesh = nullptr;

    // temp use
    D3D12_GRAPHICS_PIPELINE_STATE_DESC TempPsoDesc;

    class EditorUI* m_UI = nullptr;

	PassManager m_PassManager;



    // GI
	bool bEnableGITest = false;
	float m_GITime = 0.0f;
    std::shared_ptr<Texture> m_voxelGrid;
	int voxelGridWidth = 32;
	int voxelGridHeight = 32;
	int voxelGridDepth = 48;

    ComPtr<ID3D12Resource> m_Cascade0Buffer;
    ComPtr<ID3D12Resource> m_Cascade1Buffer;
    ComPtr<ID3D12Resource> m_Cascade2Buffer;

    DescriptorHandle m_Cascade0SRV;
    DescriptorHandle m_Cascade0UAV;
    D3D12_RESOURCE_STATES m_Cascade0State = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	CascadeResource m_CascadeResources[2][5];
    int m_currCascadeResIdx = 0;
};

