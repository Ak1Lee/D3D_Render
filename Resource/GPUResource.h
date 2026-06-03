#pragma once
#include <string>
#include <d3d12.h>
#include <wrl.h>
#include "d3dx12.h"
#include "dxUtils.h"
#include <DescriptorAllocator.h>

class GPUResource
{
public: 
	GPUResource() = default;
	GPUResource(const std::string& InName);
	virtual ~GPUResource() = default;

	// no copy construct
	GPUResource(const GPUResource&) = delete;
	GPUResource& operator=(const GPUResource&) = delete;

	// move
	GPUResource(GPUResource&&) = default;
	GPUResource& operator=(GPUResource&&) = default;


	ID3D12Resource* GetD3DResource() const { return m_Resource.Get(); }
	ID3D12Resource** GetD3DResourceAddress() { return m_Resource.GetAddressOf(); }

	D3D12_RESOURCE_STATES GetCurrentState() const { return m_CurrentState; }

	void TransitionTo(ID3D12GraphicsCommandList* CmdList, D3D12_RESOURCE_STATES TargetState);

	// SRV
	bool HasSRV();
	// UAV
	bool HasUAV();

	void BindSRV_Graphics(ID3D12GraphicsCommandList* CmdList, UINT RootParamIndex);
	void BindSRV_Compute(ID3D12GraphicsCommandList* CmdList, UINT RootParamIndex);
	void BindUAV_Compute(ID3D12GraphicsCommandList* CmdList, UINT RootParamIndex);



protected:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_Resource;
	D3D12_RESOURCE_STATES m_CurrentState = D3D12_RESOURCE_STATE_COMMON;
	std::string m_Name;
	DescriptorHandle m_SRVHandle = {};
	DescriptorHandle m_UAVHandle = {};

private:


};

class StructuredBuffer : public GPUResource
{
public:
	void Create(ID3D12Device* device, UINT elementCount, UINT elementSize, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS, std::string name = "default name");

	void CreateSRV(ID3D12Device* device);
	void CreateUAV(ID3D12Device* device);
	void CreateViews(ID3D12Device* device);

	UINT GetElementCount() const { return m_ElementCount; }
	UINT GetElementSize() const { return m_ElementSize; }

protected:
	UINT m_ElementCount = 0;
	UINT m_ElementSize = 0;
};


