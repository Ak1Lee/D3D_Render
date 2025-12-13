#pragma once

#include <d3d12.h>
#include <wrl.h>
#include "d3dx12.h"
#include <vector>
#include <list>

class DXRootSignature
{
public:
	DXRootSignature() = default;
	~DXRootSignature() = default;

	// add a CBV descriptor table
	void AddCBVDescriptorTable(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);


	// add a SRV descriptor table
	void AddSRVDescriptorTable(UINT shaderRegister, UINT numDescriptors, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

	// add a UAV descriptor table
	void AddUAVDescriptorTable(UINT shaderRegister, UINT numDescriptors, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

	// 添加Root Constant
	void AddRootConstant(UINT shaderRegister, UINT num32BitValues, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

	Microsoft::WRL::ComPtr<ID3D12RootSignature> Build(ID3D12Device* device, D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

private:
	std::list<CD3DX12_DESCRIPTOR_RANGE> m_ranges;          // 保存所有range
	std::vector<CD3DX12_ROOT_PARAMETER> m_rootParameters;    // 保存所有root parameter
	std::vector<D3D12_STATIC_SAMPLER_DESC> m_staticSamplers; // 保存静态采样器

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3DBlob> Signature;
};
