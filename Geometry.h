#pragma once

// 1. 首先包含 DirectX 核心库
#include <DirectXMath.h>
#include <DirectXColors.h>

// 2. D3D12 和 Windows 头文件
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include "d3dx12.h"
#include <windows.h>
#include <wrl.h>

// 3. 标准库
#include <vector>
#include <string>

// 4. 项目头文件（依赖DirectXMath）
#include "DXDevice.h"
#include "Common.h"

class Material;
class MeshBase
{
public:
	MeshBase() = default;
	virtual ~MeshBase() = default;
	MeshBase(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) {};


	// === 核心接口 ===

	virtual void InitVertexBufferAndIndexBuffer(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) = 0;
	//virtual void InitRenderResource() = 0;


	// === 缓冲区访问 ===
	virtual D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return VertexBufferView; };
	virtual D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const { return IndexBufferView; };
	virtual UINT GetVertexCount() const { return VertexCount; };
	virtual UINT GetIndexCount() const { return IndexCount; };


	// === 变换接口 ===
	virtual DirectX::XMMATRIX GetWorldMatrix();
	virtual DirectX::XMMATRIX CalMVPMatrix(DirectX::XMMATRIX ViewProj);

	// 位置
	virtual DirectX::XMFLOAT3 GetPosition() const { return Pos; };
	virtual void SetPosition(float x, float y, float z) { Pos = DirectX::XMFLOAT3(x, y, z); };
	virtual void SetPosition(DirectX::XMFLOAT3 InPos) { Pos = InPos; };

	// 旋转(欧拉角 Roll/Yaw/Pitch)
	virtual DirectX::XMFLOAT3 GetAngle() const { return Angle; };
	virtual void SetAngle(float rol, float yal, float pit) { Angle = DirectX::XMFLOAT3(rol, yal, pit); };
	virtual void SetAngle(DirectX::XMFLOAT3 InAngle) { Angle = InAngle; };

	// 缩放
	virtual DirectX::XMFLOAT3 GetScale() const { return Scale; };
	virtual void SetScale(float sx, float sy, float sz) { Scale = DirectX::XMFLOAT3(sx, sy, sz); };
	virtual void SetScale(DirectX::XMFLOAT3 InScale) { Scale = InScale; };

	// 常量缓冲区
	virtual void InitObjectConstantBuffer(ID3D12Device* Device, ID3D12DescriptorHeap* GlobalConstantBufferViewHeap, UINT descriptorSize, UINT indexInHeap);
	virtual void InitObjectConstantBuffer(ID3D12Device* Device, ID3D12DescriptorHeap* GlobalConstantBufferViewHeap, DescriptorAllocation DescriptorAllocInfo);
	virtual void UpdateObjectConstantBuffer(ObjectConstants& ObjConst);

	// 材质
	virtual void SetMaterial(Material* InMaterialPtr) { MaterialPtr = InMaterialPtr; };
	virtual void SetMaterialByName(const std::string& InMaterialName) { MaterialName = InMaterialName; };
	std::string GetMaterialName() const { return MaterialName; };

	D3D12_CPU_DESCRIPTOR_HANDLE GetCbvCpuHandle() const { return CbvCpuHandle; };
	D3D12_GPU_DESCRIPTOR_HANDLE GetCbvGpuHandle() const { return CbvGpuHandle; };

	// debug
	void SetName(const std::wstring& InName) { Name = InName; };
	const std::wstring& GetName() const { return Name; };
protected:

	DirectX::XMFLOAT3 Pos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 Angle = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };

	std::vector<StandardVertex> VertexList;
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

	// 材质
	Material* MaterialPtr = nullptr;
	std::string MaterialName;

	// === 调试 ===
	std::wstring Name = L"Unnamed Mesh";
};


class Box : public MeshBase
{
public:
	Box() = default;
	Box(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList);

	void InitVertexBufferAndIndexBuffer(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) override;
};


class Sphere : public MeshBase
{
public:
	Sphere(float radius = 1.0f, UINT slices = 40, UINT stacks = 40)
		: Radius(radius), Slices(slices), Stacks(stacks) {
		Name = L"Sphere";
	};

	void InitVertexBufferAndIndexBuffer(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) override;

private:
	float Radius;
	UINT Slices, Stacks;
};

class Plane : public MeshBase
{
public:
	Plane(float width = 1.0f, float depth = 1.0f, UINT m = 1, UINT n = 1)
		: Width(width), Depth(depth), M(m), N(n){
		Name = L"Plane";
	};
	void InitVertexBufferAndIndexBuffer(ID3D12Device* Device, ID3D12GraphicsCommandList* CommandList) override;

private:
	float Width;
	float Depth;
	UINT M;
	UINT N;
};
