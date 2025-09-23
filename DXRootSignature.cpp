#include "DXRootSignature.h"
#include "dxUtils.h"

void DXRootSignature::AddCBVDescriptorTable(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility)
{
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, shaderRegister);
	m_ranges.push_back(cbvTable);
	CD3DX12_ROOT_PARAMETER rootParam;
	rootParam.InitAsDescriptorTable(1, &m_ranges.back(), visibility);
	m_rootParameters.push_back(rootParam);
}

void DXRootSignature::AddSRVDescriptorTable(UINT shaderRegister, UINT numDescriptors, D3D12_SHADER_VISIBILITY visibility)
{
	CD3DX12_DESCRIPTOR_RANGE range;
	range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, numDescriptors, shaderRegister);

	m_ranges.push_back(range);

	CD3DX12_ROOT_PARAMETER param;
	param.InitAsDescriptorTable(1, &m_ranges.back(), visibility);

	m_rootParameters.push_back(param);
}

void DXRootSignature::AddUAVDescriptorTable(UINT shaderRegister, UINT numDescriptors, D3D12_SHADER_VISIBILITY visibility)
{
	CD3DX12_DESCRIPTOR_RANGE range;
	range.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, numDescriptors, shaderRegister);

	m_ranges.push_back(range);

	CD3DX12_ROOT_PARAMETER param;
	param.InitAsDescriptorTable(1, &m_ranges.back(), visibility);

	m_rootParameters.push_back(param);
}

void DXRootSignature::AddRootConstant(UINT shaderRegister, UINT num32BitValues, D3D12_SHADER_VISIBILITY visibility)
{
	CD3DX12_ROOT_PARAMETER param;
	param.InitAsConstants(num32BitValues, shaderRegister, 0, visibility);
	m_rootParameters.push_back(param);
}

Microsoft::WRL::ComPtr<ID3D12RootSignature> DXRootSignature::Build(ID3D12Device* device, D3D12_ROOT_SIGNATURE_FLAGS flags)
{
	CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc;
	RootSignatureDesc.Init(static_cast<UINT>(m_rootParameters.size()), m_rootParameters.data(),
		static_cast<UINT>(m_staticSamplers.size()), m_staticSamplers.data(), flags);
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;

	ThrowIfFailed(D3D12SerializeRootSignature(&RootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&serializedRootSig, &errorBlob));

	ThrowIfFailed(device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
	return rootSignature;
}
