#include "GPUResource.h"


GPUResource::GPUResource(const std::string& InName)
{
	m_Name = InName;
}


void GPUResource::TransitionTo(ID3D12GraphicsCommandList* CmdList, D3D12_RESOURCE_STATES TargetState)
{
	if (m_CurrentState != TargetState)
	{
		CD3DX12_RESOURCE_BARRIER Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_Resource.Get(),
			m_CurrentState,
			TargetState
		);
		CmdList->ResourceBarrier(1, &Barrier);
		m_CurrentState = TargetState;
	}
}

bool GPUResource::HasSRV()
{
	return m_SRVHandle.CpuHandle.ptr != 0;
}

bool GPUResource::HasUAV()
{
	return m_UAVHandle.CpuHandle.ptr != 0;
}

void GPUResource::BindSRV_Graphics(ID3D12GraphicsCommandList* CmdList, UINT RootParamIndex)
{
	TransitionTo(CmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	CmdList->SetGraphicsRootDescriptorTable(RootParamIndex, m_SRVHandle.GpuHandle);
}

void GPUResource::BindSRV_Compute(ID3D12GraphicsCommandList* CmdList, UINT RootParamIndex)
{
	TransitionTo(CmdList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	CmdList->SetComputeRootDescriptorTable(RootParamIndex, m_SRVHandle.GpuHandle);
}

void GPUResource::BindUAV_Compute(ID3D12GraphicsCommandList* CmdList, UINT RootParamIndex)
{
	TransitionTo(CmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	CmdList->SetComputeRootDescriptorTable(RootParamIndex, m_UAVHandle.GpuHandle);
}

void StructuredBuffer::Create(ID3D12Device* device, UINT elementCount, UINT elementSize, D3D12_RESOURCE_STATES initialState, std::string name)
{
	m_ElementCount = elementCount;
	m_ElementSize = elementSize;
	m_CurrentState = initialState;
	m_Name = name;

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = elementCount * elementSize;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

	device->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE,
		&desc, initialState,
		nullptr, IID_PPV_ARGS(&m_Resource));

	m_Resource->SetName(std::wstring(m_Name.begin(), m_Name.end()).c_str());
}
void StructuredBuffer::CreateSRV(ID3D12Device* device)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = m_ElementCount;
	srvDesc.Buffer.StructureByteStride = m_ElementSize;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	m_SRVHandle = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
	device->CreateShaderResourceView(m_Resource.Get(), &srvDesc, m_SRVHandle.CpuHandle);
}
void StructuredBuffer::CreateUAV(ID3D12Device* device)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = m_ElementCount;
	uavDesc.Buffer.StructureByteStride = m_ElementSize;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	m_UAVHandle = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
	device->CreateUnorderedAccessView(m_Resource.Get(), nullptr, &uavDesc, m_UAVHandle.CpuHandle);
}

void StructuredBuffer::CreateViews(ID3D12Device* device)
{
	CreateSRV(device);
	CreateUAV(device);
}

//// SRV for cascade0 (StructuredBuffer<float4>)
//m_Cascade0SRV = DescriptorAllocatorManager::GetInstance().AllocateCBV_SRV_UAV();
//D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
//srvDesc.Format = DXGI_FORMAT_UNKNOWN;
//srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
//srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//srvDesc.Buffer.FirstElement = 0;
//srvDesc.Buffer.NumElements = cnt0;
//srvDesc.Buffer.StructureByteStride = stride0;
//srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
//Device::GetInstance().GetD3DDevice()->CreateShaderResourceView(
//	m_Cascade0Buffer.Get(), &srvDesc, m_Cascade0SRV.CpuHandle);
//class Texture : public GPUResource
//{
//public:
//	void Create(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON, std::string name = "default name");
//	void CreateSRV(ID3D12Device* device);
//	void CreateUAV(ID3D12Device* device);
//	void CreateViews(ID3D12Device* device);
//	UINT GetWidth() const { return m_Width; }
//	UINT GetHeight() const { return m_Height; }
//	DXGI_FORMAT GetFormat() const { return m_Format; }
//
//protected:
//	UINT m_Width;
//	UINT m_Height;
//
//	UINT m_MipLevels = 1;
//	UINT m_ArraySize = 1;
//	DXGI_FORMAT m_Format;
//};
