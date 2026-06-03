#pragma once
#include "GPUResource.h"
template<typename T>
class ConstantBuffer : public GPUResource
{
	static_assert(std::is_trivially_copyable_v<T>, "CB 数据必须可平凡拷贝");
	static constexpr UINT kAlignedSize = (sizeof(T) + 255) & ~255;
public:

	void Create(ID3D12Device* device, const std::string name, UINT elemCount = 1)
	{

		m_ElementCount = elemCount;
		m_Name = name;
		m_CurrentState = D3D12_RESOURCE_STATE_GENERIC_READ;

		CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(kAlignedSize * elemCount);
		ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_Resource)));
		// 2) 映射得到 CPU 可写指针
		CD3DX12_RANGE ReadRange(0, 0);
		m_Resource->Map(0, &ReadRange, reinterpret_cast<void**>(&m_Mapped));

		m_Resource->SetName(std::wstring(m_Name.begin(), m_Name.end()).c_str());


	};

	void UpdateData(const T& data, UINT index = 0)
	{
		if (index >= m_ElementCount) {
			throw std::out_of_range("ConstantBuffer index out of range");
		}
		memcpy(m_Mapped + index * kAlignedSize, &data, sizeof(T));

	}

	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(UINT index = 0) const {
		return m_Resource->GetGPUVirtualAddress() + index * kAlignedSize;
	}


private:

	uint8_t* m_Mapped = nullptr;
	UINT m_ElementCount = 1;
};
