#include "DescriptorAllocator.h"
#include "dxUtils.h"

void DescriptorHeap::Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool shaderVisible)
{
    m_Type = type;
    m_Capacity = numDescriptors;
    m_ShaderVisible = shaderVisible;
    m_DescriptorSize = device->GetDescriptorHandleIncrementSize(type);

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap)));

    m_CPUStart = m_Heap->GetCPUDescriptorHandleForHeapStart();
    if (shaderVisible) {
        m_GPUStart = m_Heap->GetGPUDescriptorHandleForHeapStart();
    }
}
DescriptorHandle DescriptorHeap:: Allocate()
{
    if (m_CurrentIndex >= m_Capacity) {
        throw std::runtime_error("Descriptor heap exhausted!");
    }

    DescriptorHandle handle;
    handle.Index = m_CurrentIndex;
    handle.CpuHandle.ptr = m_CPUStart.ptr + m_CurrentIndex * m_DescriptorSize;

    if (m_ShaderVisible) {
        handle.GpuHandle.ptr = m_GPUStart.ptr + m_CurrentIndex * m_DescriptorSize;
    }

    m_CurrentIndex++;
    return handle;
}
void DescriptorHeap::Release()
{
    m_Heap.Reset();
    m_CurrentIndex = 0;
	m_FreeList.clear();

}
void DescriptorHeap::Reset()
{
    m_CurrentIndex = 0;
    m_FreeList.clear();
}

DescriptorHandle DescriptorHeap::AllocateRange(UINT count)
{
    throw std::runtime_error("to be done!");
    return DescriptorHandle();
}

//void DescriptorHeap::Free(const DescriptorHandle& handle)
//{
//    throw std::runtime_error("to be done!");
//}

void DescriptorAllocatorManager::Init(ID3D12Device* device)
{
    m_Device = device;

    m_RTVHeap = std::make_unique<DescriptorHeap>();
    m_RTVHeap->Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256, false);

    m_DSVHeap = std::make_unique<DescriptorHeap>();
    m_DSVHeap->Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64, false);

    m_CBV_SRV_UAV_Heap = std::make_unique<DescriptorHeap>();
    m_CBV_SRV_UAV_Heap->Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096, true);

    m_SamplerHeap = std::make_unique<DescriptorHeap>();
    m_SamplerHeap->Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 128, true);
}

DescriptorHandle DescriptorAllocatorManager::AllocateRTV()
{
	return m_RTVHeap->Allocate();
}

DescriptorHandle DescriptorAllocatorManager::AllocateDSV()
{
	return m_DSVHeap->Allocate();
}

DescriptorHandle DescriptorAllocatorManager::AllocateCBV_SRV_UAV()
{
	return m_CBV_SRV_UAV_Heap->Allocate();
}

ID3D12DescriptorHeap* DescriptorAllocatorManager::GetCBV_SRV_UAV_Heap() const
{
    return m_CBV_SRV_UAV_Heap->GetHeap();
}

UINT DescriptorAllocatorManager::GetRTVDescriptorSize() const
{
    return m_RTVHeap->GetDescriptorSize();
}

UINT DescriptorAllocatorManager::GetDSVDescriptorSize() const
{
    return m_DSVHeap->GetDescriptorSize();
}

UINT DescriptorAllocatorManager::GetCBV_SRV_UAV_DescriptorSize() const
{
    return m_CBV_SRV_UAV_Heap->GetDescriptorSize();
}

//DescriptorHandle DescriptorAllocatorManager::AllocateCBV_SRV_UAV_Range(UINT count)
//{
//    // todo 
//    return DescriptorHandle();
//}
void DescriptorAllocatorManager::Shutdown()
{
    m_RTVHeap->Release();
    m_DSVHeap->Release();
    m_CBV_SRV_UAV_Heap->Release();
    m_SamplerHeap->Release();
}