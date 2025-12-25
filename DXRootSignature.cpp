#include "DXRootSignature.h"
#include "dxUtils.h"

void DXRootSignature::AddCBVDescriptorTable(UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility)
{
	// CD3DX12_DESCRIPTOR_RANGE cbvTable;
	auto cbvTable = std::make_unique<CD3DX12_DESCRIPTOR_RANGE>();

	cbvTable->Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, shaderRegister);
	
	CD3DX12_ROOT_PARAMETER rootParam;
	rootParam.InitAsDescriptorTable(1, cbvTable.get(), visibility);

	m_ranges.push_back(std::move(cbvTable));
	m_rootParameters.push_back(rootParam);
}

void DXRootSignature::AddSRVDescriptorTable(UINT shaderRegister, UINT numDescriptors, D3D12_SHADER_VISIBILITY visibility)
{
	// CD3DX12_DESCRIPTOR_RANGE range;
	auto range = std::make_unique<CD3DX12_DESCRIPTOR_RANGE>();
	range->Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, numDescriptors, shaderRegister);


	CD3DX12_ROOT_PARAMETER param;
	param.InitAsDescriptorTable(1, range.get(), visibility);

	m_ranges.push_back(std::move(range));
	m_rootParameters.push_back(param);
}

void DXRootSignature::AddUAVDescriptorTable(UINT shaderRegister, UINT numDescriptors, D3D12_SHADER_VISIBILITY visibility)
{
	// CD3DX12_DESCRIPTOR_RANGE range;
	auto range = std::make_unique<CD3DX12_DESCRIPTOR_RANGE>();
	range->Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, numDescriptors, shaderRegister);



	CD3DX12_ROOT_PARAMETER param;
	param.InitAsDescriptorTable(1, range.get(), visibility);

	m_ranges.push_back(std::move(range));
	m_rootParameters.push_back(param);
}

void DXRootSignature::AddRootConstant(UINT shaderRegister, UINT num32BitValues, D3D12_SHADER_VISIBILITY visibility)
{
	CD3DX12_ROOT_PARAMETER param;
	param.InitAsConstants(num32BitValues, shaderRegister, 0, visibility);
	m_rootParameters.push_back(param);
}

void DXRootSignature::AddStaticSampler(const D3D12_STATIC_SAMPLER_DESC& samplerDesc)
{
	m_staticSamplers.push_back(samplerDesc);
}

Microsoft::WRL::ComPtr<ID3D12RootSignature> DXRootSignature::Build(ID3D12Device* device, D3D12_ROOT_SIGNATURE_FLAGS flags)
{
	CD3DX12_ROOT_SIGNATURE_DESC RootSignatureDesc;
	RootSignatureDesc.Init(static_cast<UINT>(m_rootParameters.size()), m_rootParameters.data(),
		static_cast<UINT>(m_staticSamplers.size()), m_staticSamplers.data(), flags);
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(&RootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&serializedRootSig, &errorBlob);
	if (errorBlob != nullptr)
	{
		// 把错误信息输出到 VS 的 Output 窗口
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}

	ThrowIfFailed(hr);

	ThrowIfFailed(device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
	return rootSignature;
}
