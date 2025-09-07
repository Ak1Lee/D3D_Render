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
	~MeshBase() = default;

protected:

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

	// 添加世界矩阵
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexDefaultBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexUploadBuffer;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView = {};

	unsigned int VertexBufferSize = 0;
	unsigned int VertexCount = 0;


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


private:
	

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

	//Constant Buffer View Heap
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

	BoxMesh Box;

};

