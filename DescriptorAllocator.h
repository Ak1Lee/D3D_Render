#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include <mutex>
#include "Common.h"
using Microsoft::WRL::ComPtr;

struct DescriptorHandle {
    D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = { 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = { 0 };
    UINT Index = UINT_MAX;

    bool IsValid() const { return Index != UINT_MAX; }
    void Reset() { CpuHandle.ptr = 0; GpuHandle.ptr = 0; Index = UINT_MAX; }
};
class DescriptorHeap {
public:
    DescriptorHeap() = default;
    ~DescriptorHeap() { Release(); }

    void Init(
        ID3D12Device* device,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        UINT numDescriptors,
        bool shaderVisible = false
    );

    // 重置分配器
    void Reset();
    void Release();

	DescriptorHandle Allocate();

    DescriptorHandle AllocateRange(UINT count);

    // void Free(const DescriptorHandle& handle);

    ID3D12DescriptorHeap* GetHeap() const { return m_Heap.Get(); }

    UINT GetDescriptorSize() const { return m_DescriptorSize; }
    UINT GetCapacity() const { return m_Capacity; }
    UINT GetUsedCount() const { return m_CurrentIndex; }


private:
    ComPtr<ID3D12DescriptorHeap> m_Heap;
    D3D12_DESCRIPTOR_HEAP_TYPE m_Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    D3D12_CPU_DESCRIPTOR_HANDLE m_CPUStart = { 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE m_GPUStart = { 0 };


    UINT m_NumDescriptors = 0;
    UINT m_DescriptorSize = 0;
    UINT m_Capacity = 0;
    UINT m_CurrentIndex = 0;
    bool m_ShaderVisible = false;

	// todo: improve free list management
    std::vector<UINT> m_FreeList;
};
class DescriptorAllocatorManager
{
public:
    static DescriptorAllocatorManager& GetInstance();

    // 初始化所有堆
    void Init(ID3D12Device* device);

    // 分配不同类型的描述符
    DescriptorHandle AllocateRTV();
    DescriptorHandle AllocateDSV();
    DescriptorHandle AllocateCBV_SRV_UAV();  // Shader 可见

    // 批量分配（用于 Descriptor Table）
    // DescriptorHandle AllocateCBV_SRV_UAV_Range(UINT count);

    // 获取堆（用于 SetDescriptorHeaps）
    ID3D12DescriptorHeap* GetCBV_SRV_UAV_Heap() const;

    // 获取描述符大小
    UINT GetRTVDescriptorSize() const;
    UINT GetDSVDescriptorSize() const;
    UINT GetCBV_SRV_UAV_DescriptorSize() const;

    void Shutdown();

private:
    DescriptorAllocatorManager() = default;
    ID3D12Device* m_Device = nullptr;

    // 四种类型的堆
    std::unique_ptr<DescriptorHeap> m_RTVHeap;
    std::unique_ptr<DescriptorHeap> m_DSVHeap;
    std::unique_ptr<DescriptorHeap> m_CBV_SRV_UAV_Heap;
    std::unique_ptr<DescriptorHeap> m_SamplerHeap;
};

