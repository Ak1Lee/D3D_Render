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

#include "camera.h"

constexpr unsigned int FrameBufferCount = 2;
inline unsigned int Width = 800;
inline unsigned int Height = 600;

// 常量缓冲区结构
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};


class Device
{
public:
	Device() {};
	~Device() {};

	void Init();

	IDXGIFactory4* GetDxgiFactory() { return DxgiFactory.Get(); }
	ID3D12Device* GetD3DDevice() { return D3DDevice.Get(); }

private:
	Microsoft::WRL::ComPtr<IDXGIFactory4> DxgiFactory;
	Microsoft::WRL::ComPtr<ID3D12Device> D3DDevice;
};

class MeshBase
{
public:
	MeshBase() = default;
	MeshBase(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) {};
	~MeshBase() = default;
	virtual void InitVertexBufferAndIndexBuffer(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) = 0;
	//virtual void InitRenderResource() = 0;
	virtual D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return VertexBufferView;};
	virtual D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const { return IndexBufferView; };
	virtual UINT GetVertexCount() const { return VertexCount; };
	virtual UINT GetIndexCount() const { return IndexCount; };
	virtual DirectX::XMMATRIX GetWorldMatrix();
	virtual DirectX::XMMATRIX CalMVPMatrix(DirectX::XMMATRIX ViewProj);


	virtual DirectX::XMFLOAT3 GetPosition() const { return Pos; };
	virtual void SetPosition(float x, float y, float z) { Pos = DirectX::XMFLOAT3(x, y, z); };
	virtual void SetPosition(DirectX::XMFLOAT3 InPos) { Pos = InPos; };

	virtual DirectX::XMFLOAT3 GetAngle() const { return Angle; };
	virtual void SetAngle(float rol, float yal, float pit) { Angle = DirectX::XMFLOAT3(rol, yal, pit); };
	virtual void SetAngle(DirectX::XMFLOAT3 InAngle) { Angle = InAngle; };

	virtual DirectX::XMFLOAT3 GetScale() const { return Scale; };
	virtual void SetScale(float sx, float sy, float sz) { Scale = DirectX::XMFLOAT3(sx, sy, sz); };
	virtual void SetScale(DirectX::XMFLOAT3 InScale) { Scale = InScale; };

	virtual void InitObjectConstantBuffer(ID3D12Device* Device, ID3D12DescriptorHeap* GlobalConstantBufferViewHeap,UINT descriptorSize,UINT indexInHeap);
	virtual void UpdateObjectConstantBuffer(ObjectConstants& ObjConst);
	D3D12_CPU_DESCRIPTOR_HANDLE GetCbvCpuHandle() const { return CbvCpuHandle; };
	D3D12_GPU_DESCRIPTOR_HANDLE GetGbvCpuHandle() const { return CbvGpuHandle; };
protected:

	DirectX::XMFLOAT3 Pos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 Angle = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };

	std::vector<Vertex> VertexList;
	std::vector<std::uint16_t> IndiceList;

	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexDefaultBufferGPU;
	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU;
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexUploadBuffer;

	Microsoft::WRL::ComPtr<ID3D12Resource> IndexDefaultBufferGPU;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexUploadBuffer;

	//Constant Buffer View Heap
	Microsoft::WRL::ComPtr<ID3D12Resource> ObjectConstantBuffer;
	UINT8* ConstantBufferMappedData = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE CbvCpuHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE CbvGpuHandle{};

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};

	void CreateVertexAndIndexBufferHeap(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList);

	unsigned int VertexBufferSize = 0;
	unsigned int VertexCount = 0;
	unsigned int IndexCount = 0;
};

class Box : public MeshBase
{
public:
	Box() = default;
	Box(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList);

	void InitVertexBufferAndIndexBuffer(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) override;
};

class RenderableItem
{
public:
	RenderableItem() = default;
	~RenderableItem() = default;

	void InitVertexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return VertexBufferView; }

	ID3D12Resource* GetVertexDefaultBuffer() const { return VertexDefaultBuffer.Get(); }

	UINT GetVertexCount() const { return VertexCount; }


private:
	std::vector<Vertex> VertexList;

	// Model Matrix
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexDefaultBuffer;
	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU;
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexUploadBuffer;


	Microsoft::WRL::ComPtr<ID3D12Resource> IndexDefaultBuffer;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexUploadBuffer;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};

	unsigned int VertexBufferSize = 0;
	unsigned int VertexCount = 0;
	unsigned int IndexCount = 0;

};

class BoxMesh
{
	public:
	BoxMesh() = default;
	~BoxMesh() = default;

	void InitVertexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return VertexBufferView; }
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const { return IndexBufferView; }

	ID3D12Resource* GetVertexDefaultBuffer() const { return VertexDefaultBuffer.Get(); }

	UINT GetVertexCount() const { return VertexCount; }

	UINT GetIndexCount() const { return IndexCount; }

	DirectX::XMMATRIX GetWorldMatrix();

	DirectX::XMMATRIX CalMVPMatrix(DirectX::XMMATRIX ViewProj);


private:
	std::vector<Vertex> VertexList;
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexDefaultBuffer;
	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU;
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexUploadBuffer;


	Microsoft::WRL::ComPtr<ID3D12Resource> IndexDefaultBuffer;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexUploadBuffer;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW IndexBufferView = {};

	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();


	unsigned int VertexBufferSize = 0;
	unsigned int VertexCount = 0;
	unsigned int IndexCount = 0;
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

    void CreateFence();

    void Draw();

    ~DXRender();

    Camera& GetMainCamera() { return MainCamera; }

private:
    DXRender();

    Device DxDevice;

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

    RenderableItem Triangle;

    BoxMesh BoxShape;

	std::vector<MeshBase*> MeshList;

	MeshBase* PtrMesh = nullptr;

};

